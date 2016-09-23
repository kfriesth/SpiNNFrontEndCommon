from spinn_front_end_common.utility_models.abstract_packet_io_machine_vertex import AbstractPacketIOMachineVertex
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


class PRAERIEReceiverMachineVertex(
        AbstractPacketIOMachineVertex, AbstractRecordable):

      def __init__(self, label, constraints=None,
            board_address=None,
            stimulus_onset=0,
            simulation_time=0,
            tick_period=1000,
            default_tag=0,
            infinite_run=False,
            sequence_tags=None,
            instrumented=False,
            SDRAM_blocks=None,
            SDP_receive_port=0,
            iptags=None,
            reverse_iptags=None):

     @overrides(
        AbstractPacketIOMachineVertex.generate_data_specification,
        additional_arguments={
            "machine_graph", "routing_info"})
      def generate_data_specification(self, spec, placement,
            machine_time_step, time_scale_factor, machine_graph,
            routing_info, tags):
