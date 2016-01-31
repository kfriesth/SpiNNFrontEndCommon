# spinn_io_handler imports
from spinn_storage_handlers.file_data_reader import FileDataReader

# data spec imports
import data_specification.data_spec_sender.spec_sender as spec_sender

# pacman imports
from pacman.utilities.utility_objs.progress_bar import ProgressBar

# spinnman imports
from spinnman.model.core_subsets import CoreSubsets
from spinnman.model.cpu_state import CPUState
from multiprocessing import Process
# front end common imports
from spinn_front_end_common.abstract_models.\
    abstract_data_specable_vertex import \
    AbstractDataSpecableVertex
from spinn_front_end_common.utilities import constants
from spinn_front_end_common.utilities import exceptions

import os
import logging
import struct
import time

logger = logging.getLogger(__name__)


class FrontEndCommonPartitionableGraphMachineExecuteDataSpecification(object):
    """ Executes the machine based data specification
    """

    def __call__(
            self, hostname, placements, graph_mapper, report_default_directory,
            report_states, runtime_application_data_folder, machine,
            board_version, dsg_targets, transceiver, dse_app_id, app_id, lst):
        """

        :param hostname:
        :param placements:
        :param graph_mapper:
        :param write_text_specs:
        :param runtime_application_data_folder:
        :param machine:
        :return:
        """
        data = self.spinnaker_based_data_specification_execution(
            hostname, placements, graph_mapper, report_states,
            runtime_application_data_folder, machine, board_version,
            report_default_directory, dsg_targets, transceiver,
            dse_app_id, app_id,lst)

        return data

    @staticmethod
    def send(transceiver, core_pk_list_creator):
        time.sleep(0.1)
        from spinnman.messages.sdp.sdp_message import SDPMessage
        pkt_lst_creator=core_pk_list_creator
        pk_list=core_pk_list_creator.stored_packets
        for i in pk_list:
            transceiver.send_sdp_message(SDPMessage(pkt_lst_creator.header, i.bytestring))
            time.sleep(0.01)
            #time.sleep(throttling_ms)


    def spinnaker_based_data_specification_execution(
            self, hostname, placements, graph_mapper, report_states,
            application_data_runtime_folder, machine, board_version,
            report_default_directory, dsg_targets, transceiver,
            dse_app_id, app_id, lst):
        """

        :param hostname:
        :param placements:
        :param graph_mapper:
        :param write_text_specs:
        :param application_data_runtime_folder:
        :param machine:
        :return:
        """
        mem_map_report = report_states.write_memory_map_report

        # check which cores are in use
        number_of_cores_used = 0
        core_subset = CoreSubsets()
        for placement in placements.placements:
            core_subset.add_processor(placement.x, placement.y, placement.p)
            number_of_cores_used += 1

        # read DSE exec name
        executable_targets = {
            os.path.dirname(spec_sender.__file__) +
            '/data_specification_executor.aplx': core_subset}

        self._load_executable_images(
           transceiver, executable_targets, dse_app_id,
           app_data_folder=application_data_runtime_folder)

        # create a progress bar for end users
        progress_bar = ProgressBar(len(list(placements.placements)),
                                   "Loading data specifications on chip")

        processes=list()
        for placement in placements.placements:
            associated_vertex = graph_mapper.get_vertex_from_subvertex(
                placement.subvertex)
            # if the vertex can generate a DSG, call it
            if isinstance(associated_vertex, AbstractDataSpecableVertex):
                x, y, p = placement.x, placement.y, placement.p
                k_gen=str(x)+str(y)+str(p)
                pack_list=lst[k_gen];
                #p = Process(target=self.send, args=(transceiver, pack_list, ))
                #processes.append(p)
                self.send(transceiver, pack_list)
        '''
        for process in processes:
            process.start()
        for process in processes:
            process.join()
            progress_bar.update()
        '''
        progress_bar.end()

        time.sleep(20)

        transceiver.stop_application(dse_app_id)
        logger.info("On-chip data spec executor completed")

        return {"LoadedApplicationDataToken": True,
                "DSEOnHost": False,
                "DSEOnChip": True}

    def _load_executable_images(self, transceiver, executable_targets, app_id,
                                app_data_folder):
        """ Go through the executable targets and load each binary to \
            everywhere and then send a start request to the cores that \
            actually use it
        """
        #if self._reports_states.transciever_report:
        #    reports.re_load_script_load_executables_init(
        #        app_data_folder, executable_targets)

        progress_bar = ProgressBar(len(executable_targets),
                                   "Loading executables onto the machine")
        for executable_target_key in executable_targets:
            file_reader = FileDataReader(executable_target_key)
            core_subset = executable_targets[executable_target_key]

            statinfo = os.stat(executable_target_key)
            size = statinfo.st_size

            # TODO there is a need to parse the binary and see if its
            # ITCM and DTCM requirements are within acceptable params for
            # operating on spinnaker. Currently there are just a few safety
            # checks which may not be accurate enough.
            if size > constants.MAX_SAFE_BINARY_SIZE:
                logger.warn(
                    "The size of {} is large enough that its"
                    " possible that the binary may be larger than what is"
                    " supported by spinnaker currently. Please reduce the"
                    " binary size if it starts to behave strangely, or goes"
                    " into the wdog state before starting.".format(
                        executable_target_key))
                if size > constants.MAX_POSSIBLE_BINARY_SIZE:
                    raise exceptions.ConfigurationException(
                        "The size of the binary is too large and therefore"
                        " will very likely cause a WDOG state. Until a more"
                        " precise measurement of ITCM and DTCM can be produced"
                        " this is deemed as an error state. Please reduce the"
                        " size of your binary or circumvent this error check.")

            transceiver.execute_flood(core_subset, file_reader, app_id,
                                      size)

#            if self._reports_states.transciever_report:
#                reports.re_load_script_load_executables_individual(
#                    app_data_folder, executable_target_key,
#                    app_id, size)
            progress_bar.update()
        progress_bar.end()
