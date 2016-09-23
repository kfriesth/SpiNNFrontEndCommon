from spinn_front_end_common.abstract_models\
    .abstract_generates_data_specification \
    import AbstractGeneratesDataSpecification
from spinn_front_end_common.abstract_models.abstract_has_associated_binary \
    import AbstractHasAssociatedBinary
from pacman.model.abstract_classes.abstract_has_constraints \
    import AbstractHasConstraints
from spinn_front_end_common.abstract_models\
    .abstract_binary_uses_simulation_run import AbstractBinaryUsesSimulationRun
from pacman.model.graphs.machine.abstract_machine_vertex \
    import AbstractMachineVertex

from pacman.model.constraints.placer_constraints.placer_board_constraint\
    import PlacerBoardConstraint
from pacman.model.resources.resource_container import ResourceContainer
from pacman.model.resources.dtcm_resource import DTCMResource
from spinn_front_end_common.utilities import constants

from enum import Enum

class AbstractPacketIOMachineVertex(
        AbstractMachineVertex, AbstractHasAssociatedBinary,
        AbstractGeneratesDataSpecification, AbstractHasConstraints,
        AbstractBinaryUsesSimulationRun):

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

      @delegates_to("_constraints", ConstrainedObject.add_constraint)
      def add_constraint(self, constraint):
          pass

      @delegates_to("_constraints", ConstrainedObject.add_constraints)
      def add_constraints(self, constraints):
          pass

      @delegates_to("_constraints", ConstrainedObject.constraints)
      def constraints(self):
          pass

      @property
      @overrides(AbstractMachineVertex.label)
      def label(self):
          return self._label

      @property
      @overrides(AbstractMachineVertex.resources_required)
      def resources_required(self):
          return ResourceContainer(
              dtcm=DTCMResource(self.get_dtcm_usage()),
              sdram=SDRAMResource(self.get_sdram_usage()),
              cpu_cycles=CPUCyclesPerTickResource(self.get_cpu_usage()),
              iptags=self.get_iptags(),
              reverse_iptags=self.get_reverse_iptags())

      @abstractmethod
      def get_sdram_usage():

      @abstractmethod
      def get_dtcm_usage():

      @abstractmethod
      def get_cpu_usage():

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
