from spinn_front_end_common.utility_models.packet_io_machine_vertex import PacketIOMachineVertex
from pacman.model.constraints.key_allocator_constraints\
    .key_allocator_fixed_key_and_mask_constraint \
    import KeyAllocatorFixedKeyAndMaskConstraint
from pacman.model.resources.iptag_resource import IPtagResource
from pacman.model.resources.reverse_iptag_resource import ReverseIPtagResource
from pacman.model.constraints.placer_constraints\
    .placer_board_constraint import PlacerBoardConstraint
from pacman.model.resources.iptag_resource import IPtagResource
from pacman.model.resources.reverse_iptag_resource import ReverseIPtagResource
from pacman.model.routing_info.base_key_and_mask import BaseKeyAndMask

from spinn_front_end_common.abstract_models.abstract_recordable \
    import AbstractRecordable
# somewhat inelegant need to import this odd module to implement some
# aspects of recording functionality. Not the most informative module
# name nor logical place to put this but there it is.
from spinn_front_end_common.interface.buffer_management.buffer_models \
    import receives_buffers_to_host_basic_impl

class PRAERIEReceiverMachineVertex(
        PacketIOMachineVertex, AbstractRecordable):

      def __init__(self, label, constraints=None,
            board_address=None,
            stimulus_onset=0,
            simulation_time=0,
            tick_period=1000,
            default_tag=0,
            infinite_run=False,
            sequence_tags=None,
            instrumented=False,
            SDRAM_blocks=(0,0),
            record_space=(0,0),
            SDP_receive_port=0,
            iptags=None,
            reverse_iptags=None):
 
            AbstractPacketIOMachineVertex.__init__(label, constraints,
            board_address, stimulus_onset, simulation_time, tick_period,
            default_tag, infinite_run, sequence_tags,
            instrumented)

            self._constraints = ConstrainedObject(constraints)

            if board_address is not None:
                self.add_constraint(PlacerBoardConstraint(board_address))

            self._setup_reverse_ip_tags(reverse_iptags, self._sequence_tags,
                  SDP_receive_port)
            # sets the number of SDRAM blocks for spike buffering of incoming
            # spikes. A block is a linked list of entries, max length 
            # SPIKE_BUF_MAX_SPIKES (for which see PRAERIE_constants.py in 
            # utilities). This is a tuple of 2 entries; the first is the
            # number of blocks for spikes without payloads, the second for
            # spikes with payloads.
            self._sdram_blocks = SDRAM_blocks
            self._record_space = record_space

     def _setup_reverse_ip_tags(self, reverse_iptags, seq_tags, sdp_port):
         # creates the list of tags handled by this vertex. (The list should
         # be small, no more than about 4 entries)
         self._reverse_iptags = [] # blank start list of tags
         # Now set up the tags proper
         if reverse_iptags is not None:
            # reverse_iptags should be a dictionary of (tag, port) pairs
            for tag in reverse_iptags:
                # add the tag and also the sequence tag, if there is a matching
                # one. Provided sequence tags that aren't mapped to any actual 
                # tag on this vertex are not added. This allows a single sequence
                # tag list to be input without having to worry about splitting it
                # if the mapper decides to split reverse_iptags between vertices.
                self._reverse_iptags.append([ReverseIPtagResource(
                    port=reverse_iptags[tag], sdp_port=SDP_receive_port,
                    tag=tag)])
                if tag in sequence_tags:
                    self._seq_tags_list.append(tag)
 
     @overrides(
        AbstractPacketIOMachineVertex.generate_data_specification,
        additional_arguments={"machine_graph", "routing_info"})
     def generate_data_specification(self, spec, placement,
         machine_time_step, time_scale_factor, machine_graph,
         routing_info, tags):

     def get_sdram_usage():
         if not self._sdram_needed:
            self._sdram_neeeded = PacketIOMachineVertex.get_sdram_usage()
         self._sdram_needed += SPIKE_LINKED_LIST_ITEM_SIZE * SPIKE_BUF_MAX_SPIKES * self._sdram_blocks[0]
         self._sdram_needed += SPIKE_LINKED_LIST_ITEM_SIZE * SPIKE_BUF_MAX_SPIKES * self._sdram_blocks[1]
         sdram_extra_regions = 0
         if self._instrumented:
            if self._sdram_blocks[0] and self._record_space[0]: 
               sdram_extra_regions += 1
            if self._sdram_blocks[1] and self._record_space[1]: 
               sdram_extra_regions += 1
            self._sdram_needed += self.get_recording_data_size(
                                       sdram_extra_regions)
            if self._record_space[0] or self._record_space[1]:
               if self._buffered_sdram_per_timestep:
                  self._sdram_needed += self._minimum_sdram_for_buffering
               else:
                  self._sdram_needed += (self._record_space[0] +
                                         self._record_space[1])
                  self._sdram_needed += self.get_buffer_state_region_size(
                                        sdram_extra_regions)
         if self._sdram_blocks[0]: sdram_extra_regions += 1
         if self._sdram_blocks[1]: sdram_extra_regions += 1
         self._sdram_needed += sdram_extra_regions * SARK_PER_MALLOC_SDRAM_USAGE
         

     def get_dtcm_usage():

     def get_cpu_usage():
