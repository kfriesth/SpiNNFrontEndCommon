from six import add_metaclass
from abc import ABCMeta
from abc import abstractmethod


@add_metaclass(ABCMeta)
class AbstractUsesCustomDataExpander(object):
    """ Indicates that the vertex has its own data expander, which generates\
        data and needs to be executed before the vertex executable itself.
        Note that all data expanders will be executed synchronously.
    """
    
    @abstractmethod
    def get_data_expander_executable(self):
        """ Get the name of the executable used to expand the data.
            The expander is expected to be found with an ExecutableFinder
            
        :return: The name of an executable
        :rtype: str
        """
    
    @abstractmethod
    def write_data_expander_data(self, file_writer):
        """ Write the data required by the expander
        
        :param file_writer: The file to write the data to
        :type file_writer: `py:class:spinn_storage_handlers.FileW`
        """
