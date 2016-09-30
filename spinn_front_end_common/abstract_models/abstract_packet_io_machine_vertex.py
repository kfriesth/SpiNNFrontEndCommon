from pacman.model.abstract_classes.abstract_has_constraints \
    import AbstractHasConstraints
from pacman.model.graphs.machine.impl.machine_vertex \
    import MachineVertex

from pacman.model.decorators.overrides import overrides
from pacman.model.resources.resource_container import ResourceContainer
from pacman.model.resources.dtcm_resource import DTCMResource
from pacman.model.resources.sdram_resource import SDRAMResource
from pacman.model.resources.cpu_cycles_per_tick_resource \
    import CPUCyclesPerTickResource 


class AbstractPacketIOMachineVertex(MachineVertex, AbstractHasConstraints):

      @property
      @overrides(MachineVertex.resources_required)
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

      @abstractmethod
      def get_iptags():

      @abstractmethod
      def get_reverse_iptags():
