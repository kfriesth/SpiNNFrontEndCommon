from spinn_front_end_common.abstract_models\
    .abstract_generates_data_specification \
    import AbstractGeneratesDataSpecification
from spinn_front_end_common.abstract_models.abstract_has_associated_binary \
    import AbstractHasAssociatedBinary
from spinn_front_end_common.abstract_models\
    .abstract_binary_uses_simulation_run import AbstractBinaryUsesSimulationRun
from spinn_front_end_common.models.abstract_models \
    import AbstractPacketIOMachineVertex
from spinn_front_end_common.interface.provenance\
    .provides_provenance_data_from_machine_impl \
    import ProvidesProvenanceDataFromMachineImpl

from pacman.model.decorators.overrides import overrides
from pacman.executor.injection_decorator import inject_items
from pacman.model.constraints.placer_constraints.placer_board_constraint\
    import PlacerBoardConstraint
from pacman.model.resources.resource_container import ResourceContainer
from pacman.model.resources.dtcm_resource import DTCMResource
from pacman.model.resources.dtcm_resource import DTCMResource
from pacman.model.resources.sdram_resource import SDRAMResource
from pacman.model.resources.cpu_cycles_per_tick_resource \
    import CPUCyclesPerTickResource 
from spinn_front_end_common.utilities import constants
from spinn_front_end_common.utilities import PRAERIE_constants

from enum import Enum

class PacketIOMachineVertex(
        AbstractPacketIOMachineVertex, AbstractHasAssociatedBinary,
        AbstractGeneratesDataSpecification AbstractBinaryUsesSimulationRun, 
        ProvidesProvenanceDataFromMachineImpl):

      def __init__(
            self, label, constraints=None,
            board_address=None,
            stimulus_onset=0,
            simulation_time=0,
            tick_period=1000,
            default_tag=0,
            infinite_run=False,
            sequence_tags=None,
            instrumented=False):

            self._label = label
            self._board_address = board_address
            self._stimulus_onset = stimulus_onset
            self._simulation_time = simulation_time
            self._tick_period = tick_period
            self._default_tag = default_tag
            self._infinite_run = infinite_run
            self._sequence_tags = sequence_tags
            self._instrumented = instrumented
            self._seq_tag_list = []
            self._sdram_needed = 0
            self._dtcm_needed = 0
            self._cpu_needed = 0

      @property
      @overrides(AbstractPacketIOMachineVertex.resources_required)
      def resources_required(self):
          return ResourceContainer(
              dtcm=DTCMResource(self.get_dtcm_usage()),
              sdram=SDRAMResource(self.get_sdram_usage()),
              cpu_cycles=CPUCyclesPerTickResource(self.get_cpu_usage()),
              iptags=self.get_iptags(),
              reverse_iptags=self.get_reverse_iptags())

      @overrides(AbstractPacketIOMachineVertex.get_sdram_usage)
      def get_sdram_usage():

      @overrides(AbstractPacketIOMachineVertex.get_dtcm_usage)
      def get_dtcm_usage():

      @overrides(AbstractPacketIOMachineVertex.get_cpu_usage)            
      def get_cpu_usage():

      @overrides(AbstractPacketIOMachineVertex.get_iptags) 
      def get_iptags():

      @overrides(AbstractPacketIOMachineVertex.get_reverse_iptags) 
      def get_reverse_iptags():

      @inject_items({
          "machine_time_step": "MachineTimeStep",
          "time_scale_factor": "TimeScaleFactor",
          "tags": "MemoryTags"})
      @overrides(
          AbstractGeneratesDataSpecification.generate_data_specification,
          additional_arguments={
              "machine_time_step", "time_scale_factor", "tags"
           })
      def generate_data_specification(
          self, spec, placement, machine_time_step, time_scale_factor,
          tags):

          # Construct the data images needed for the Neuron:
          self._reserve_regions(spec)
          self._write_setup_info(spec, machine_time_step, time_scale_factor)
          self._write_configuration(
              spec, tags.get_ip_tags_for_vertex(self))

          # End-of-Spec:
          spec.end_specification()
