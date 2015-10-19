# spinnmachine imports
from spinn_machine.virutal_machine import VirtualMachine


class FrontEndCommonVirtualMachineInterfacer(object):
    """
    FrontEndCommonMachineInterfacer: interface to make a transciever and a
    spinnmachine object
    """

    def __call__(self, width, height, virtual_has_wrap_arounds):
        """
        Set up the interfaces for communicating with the SpiNNaker board
        :param width: The width of the machine in chips
        :param height: The height of the machine in chips
        :param virtual_has_wrap_arounds: True if the machine is virtual and\
                should be created with wrap_arounds
        :return: None
        """

        machine = VirtualMachine(
            width=width, height=height,
            with_wrap_arounds=virtual_has_wrap_arounds)

        return {"machine": machine}