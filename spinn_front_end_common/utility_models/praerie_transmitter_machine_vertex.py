from spinn_front_end_common.utility_models.packet_io_machine_vertex import PacketIOMachineVertex
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

      """
      A method to create the PRAERIE lookup table needed to associate MC 
      packets with outgoing PRAERIE packet headers. Parameters:
      keys: a list of (first, last) key ranges to be associated with a given
      entry. All keys in the range will be mapped to a table entry.
      payloads: a list of payload prefixes (if any) to be applied to the
      key range. If an entry is None it indicates no payload prefix
      timestamps: a list of timestamp prefixes, as with payload prefixes
      PRAERIE_types: a list of dictionaries of specifications for the various
      PRAERIE parameters. EIEIO types may also be specified, if key protocol_type
      has value EIEIO.

      The method builds the table by setting an appropriate mask to capture
      the bit-space of the key range (possibly splitting in 2), then builds
      the coded PRAERIE header by looking at all the entries in the PRAERIE_types
      dictionary. This table should then be inserted into variable hdr_lookup_cam
      in the core code.
      """
      def _generate_PRAERIE_hdr_table(self, keys=None, payloads=None, timestamps=None, PRAERIE_types=None):
