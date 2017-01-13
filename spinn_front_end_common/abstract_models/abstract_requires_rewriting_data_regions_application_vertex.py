from six import add_metaclass
from abc import ABCMeta
from abc import abstractmethod


@add_metaclass(ABCMeta)
class AbstractRequiresRewriteDataRegionsApplicationVertex(object):

    @abstractmethod
    def regions_and_data_spec_to_rewrite(
            self, placement, hostname, report_directory, write_text_specs,
            reload_application_data_file_path, graph_mapper):
        """ method for getting regions that need to be rewritten between runs
        :return: a dict of data regions and
        """

    @abstractmethod
    def requires_memory_regions_to_be_reloaded(self):
        """ allows a flag check on if this functionality will be needed

        :return:
        """