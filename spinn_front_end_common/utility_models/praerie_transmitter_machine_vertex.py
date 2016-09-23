from spinn_front_end_common.utility_models.abstract_packet_io_machine_vertex import AbstractPacketIOMachineVertex
from pacman.model.constraints.placer_constraints.placer_board_constraint\
    import PlacerBoardConstraint
from pacman.model.constraints.placer_constraints\
    .placer_radial_placement_from_chip_constraint \
    import PlacerRadialPlacementFromChipConstraint
from pacman.model.resources.iptag_resource import IPtagResource
from spinn_front_end_common.utilities.utility_objs.provenance_data_item \
    import ProvenanceDataItem

class PRAERIETransmitterMachineVertex(AbstractPacketIOMachineVertex):

      def __init__(self, label, constraints=None,
            board_address=None,
            stimulus_onset=0,
            simulation_time=0,
            tick_period=1000,
            default_tag=0,
            infinite_run=False,
            sequence_tags=None,
            instrumented=False,
            iptags=None,
            send_buffer_size=0,
            MC_tag_table=None):
