from pacman.utilities.utility_objs.progress_bar import ProgressBar
from spinn_front_end_common.abstract_models.\
    abstract_data_specable_vertex import AbstractDataSpecableVertex
from spinn_front_end_common.utilities.executable_targets import \
    ExecutableTargets
from spinn_front_end_common.utilities import exceptions

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
import time
import logging
logger=logging.getLogger(__name__)

class FrontEndCommomPartitionableGraphDataSpecificationWriterAndSender(object):
    """ Executes a partitionable graph data specification generation
    """

    def __call__(
            self, placements, graph_mapper, tags, executable_finder,
            partitioned_graph, partitionable_graph, routing_infos, hostname,
            report_default_directory, write_text_specs,
            app_data_runtime_folder, transceiver):
        """ generates the dsg for the graph.

        :return:
        """

        # iterate though subvertices and call generate_data_spec for each
        # vertex
        number_of_cores_used = 0
        core_subset = CoreSubsets()
        for placement in placements.placements:
            core_subset.add_processor(placement.x, placement.y, placement.p)
            number_of_cores_used += 1

        from data_specification.sender_pool import SenderPool

        # read DSE exec name
        executable_targets = {
            os.path.dirname(spec_sender.__file__) +
            '/data_specification_executor.aplx': core_subset}

        self._load_executable_images(transceiver, executable_targets, 31)


        executable_targets = ExecutableTargets()
        dsg_targets = dict()
        lst=dict()
        #logger.info("test")
        # create a progress bar for end users
        progress_bar = ProgressBar(len(list(placements.placements)),
                                   "Generating and Asyncronously Sending data specifications")
        sp=SenderPool(1, transceiver)
        queue=sp.get_queue()
        for placement in placements.placements:
            associated_vertex = graph_mapper.get_vertex_from_subvertex(
                placement.subvertex)

            # if the vertex can generate a DSG, call it
            if isinstance(associated_vertex, AbstractDataSpecableVertex):

                strkey=str(placement.x)+str(placement.y)+str(placement.p)
                ip_tags = tags.get_ip_tags_for_vertex(
                    placement.subvertex)
                reverse_ip_tags = tags.get_reverse_ip_tags_for_vertex(
                    placement.subvertex)
                try:
                    if strkey=="003": #or strkey=="003"
                        pass
                    file_path = associated_vertex.generate_data_spec(
                        placement.subvertex, placement, partitioned_graph,
                        partitionable_graph, routing_infos, hostname, graph_mapper,
                        report_default_directory, ip_tags, reverse_ip_tags,
                        write_text_specs, app_data_runtime_folder, send_async=True, queue=queue)
                except:
                    logger.info(strkey)
                    #logger.info(e)
                    break

                #lst[strkey]=packet_list

                # link dsg file to subvertex
                dsg_targets[placement.x, placement.y, placement.p,
                            associated_vertex.label] = file_path

                # Get name of binary from vertex
                binary_name = associated_vertex.get_binary_file_name()

                # Attempt to find this within search paths
                binary_path = executable_finder.get_executable_path(
                    binary_name)
                if binary_path is None:
                    raise exceptions.ExecutableNotFoundException(binary_name)

                if not executable_targets.has_binary(binary_path):
                    executable_targets.add_binary(binary_path)
                executable_targets.add_processor(
                    binary_path, placement.x, placement.y, placement.p)

            progress_bar.update()


        time.sleep(2)
        sp.stop()
        # finish the progress bar
        progress_bar.end()
        logger.info("ended spec")
        transceiver.stop_application(31)
        '''
        import pickle
        serialized_brunnell = open('serialized_brunnell', 'wb')
        pickle.dump(lst, serialized_brunnell)
        time.sleep(100)
        serialized_brunnell.close()
        '''
        return {'executable_targets': executable_targets,
                'dsg_targets': dsg_targets,
                "LoadedApplicationDataToken": True,
                "DSEOnHost": False,
                "DSEOnChip": True}


    def _load_executable_images(self, transceiver, executable_targets, app_id):
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
