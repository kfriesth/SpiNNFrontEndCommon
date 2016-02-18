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
from multiprocessing import Queue
import os
import time
import struct
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

        # check which cores are in use
        number_of_cores_used = 0
        core_subset = CoreSubsets()
        for placement in placements.placements:
            associated_vertex = graph_mapper.get_vertex_from_subvertex(
                placement.subvertex)
            if isinstance(associated_vertex, AbstractDataSpecableVertex):
                core_subset.add_processor(placement.x, placement.y, placement.p)

        # read DSE exec name
        executable_targets = {
            os.path.dirname(spec_sender.__file__) + '/no_async'+
            '/data_specification_executor_no_async.aplx': core_subset}

        executable_targets=ExecutableTargets()
        q=Queue()
        p=Process(target=self.send_data_async, args=(q, transceiver, 31, 0))
        p.start()
        dsg_targets = dict()
        progress_bar = ProgressBar(len(list(placements.placements)),
                                   "Generating Asyncronously Sending data specifications via SCP")

        for placement in placements.placements:
            associated_vertex = graph_mapper.get_vertex_from_subvertex(
                placement.subvertex)

            # if the vertex can generate a DSG, call it
            if isinstance(associated_vertex, AbstractDataSpecableVertex):

                ip_tags = tags.get_ip_tags_for_vertex(
                    placement.subvertex)
                reverse_ip_tags = tags.get_reverse_ip_tags_for_vertex(
                    placement.subvertex)
                try:
                    file_path = associated_vertex.generate_data_spec(
                        placement.subvertex, placement, partitioned_graph,
                        partitionable_graph, routing_infos, hostname, graph_mapper,
                        report_default_directory, ip_tags, reverse_ip_tags,
                        write_text_specs, app_data_runtime_folder)
                except:
                    logger.error("Something bad happened during the generation and sending of core x: "+str(placement.x)+" y: "+str(placement.y)+" p:"+str(placement.p) )
                    break

                # link dsg file to subvertex
                dsg_targets[placement.x, placement.y, placement.p,
                            associated_vertex.label] = file_path

                q.put([file_path, placement.x, placement.y, placement.p, 30])
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

        q.put(["stop"])
        p.join()


        processors_exited = transceiver.get_core_state_count(
            31, CPUState.FINISHED)
        while processors_exited < number_of_cores_used:
            logger.info("Data spec executor on chip not completed, waiting for it to complete")
            time.sleep(1)
            processors_exited = transceiver.get_core_state_count(
                31, CPUState.FINISHED)
        progress_bar.end()
        logger.info("ended spec")
        transceiver.stop_application(31)

        return {'executable_targets': executable_targets,
                'dsg_targets': dsg_targets,
                "LoadedApplicationDataToken": True,
                "DSEOnHost": False,
                "DSEOnChip": True}

    @staticmethod
    def send_data_async(queue, transceiver, dse_app_id, mem_map_report):
        ctr=0
        while True:
            value=queue.get() # [filename, x, y, p, app_id]
            ctr += 1
            data_spec_file_path=value[0]
            if data_spec_file_path == "stop":
                return
            x=value[1]
            y=value[2]
            p=value[3]
            app_id=value[4]
            cs=CoreSubsets()
            cs.add_processor(x,y,p)
            dse_data_struct_addr = transceiver.malloc_sdram(
                        x, y, constants.DSE_DATA_STRUCT_SIZE, dse_app_id)

            data_spec_file_size = os.path.getsize(data_spec_file_path)

            application_data_file_reader = FileDataReader(
                data_spec_file_path)

            base_address = transceiver.malloc_sdram(
                x, y, data_spec_file_size, dse_app_id)

            dse_data_struct_data = struct.pack(
                    "<IIII",
                    base_address,
                    data_spec_file_size,
                    app_id,
                    mem_map_report)

            transceiver.write_memory(
                x, y, dse_data_struct_addr, dse_data_struct_data,
                len(dse_data_struct_data))

            transceiver.write_memory(
                x, y, base_address, application_data_file_reader,
                data_spec_file_size)

            # data spec file is written at specific address (base_address)
            # this is encapsulated in a structure with four fields:
            # 1 - data specification base address
            # 2 - data specification file size
            # 3 - future application ID
            # 4 - store data for memory map report (True / False)
            # If the memory map report is going to be produced, the
            # address of the structure is returned in user1
            user_0_address = transceiver.\
                get_user_0_register_address_from_core(x, y, p)

            transceiver.write_memory(
                x, y, user_0_address, dse_data_struct_addr, 4)
            statinfo = os.stat( os.path.dirname(spec_sender.__file__) + '/no_async'+'/data_specification_executor_no_async.aplx')
            size = statinfo.st_size
            file_reader = FileDataReader(os.path.dirname(spec_sender.__file__) + '/no_async'+'/data_specification_executor_no_async.aplx')
            transceiver.execute_flood(cs, file_reader, 31, size)
            processors_exited = transceiver.get_core_state_count(
            31, CPUState.FINISHED)

            while processors_exited < ctr:
                #logger.info("Data spec executor on chip not completed, waiting for it to complete")
                time.sleep(0.1)
                processors_exited = transceiver.get_core_state_count(
                    31, CPUState.FINISHED)




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
