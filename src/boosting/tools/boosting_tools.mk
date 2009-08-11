LIBBOOSTING_TOOLS_SOURCES := \
        boosting_tool_common.cc \
	datasets.cc

LIBBOOSTING_TOOLS_LINK :=	utils db arch boost_regex-mt

$(eval $(call library,boosting_tools,$(LIBBOOSTING_TOOLS_SOURCES),$(LIBBOOSTING_TOOLS_LINK)))


$(eval $(call program,classifier_training_tool,boosting boosting_tools utils ACE boost_program_options-mt boost_regex-mt $(if $(findstring 1,$(CUDA_ENABLED)), boosting_cuda),,tools))

$(eval $(call program,training_data_tool,boosting boosting_tools utils ACE boost_program_options-mt boost_regex-mt,,tools))

