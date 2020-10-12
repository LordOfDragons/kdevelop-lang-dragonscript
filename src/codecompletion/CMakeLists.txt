set(codecompletion_STAT_SRCS
	DSCodeCompletionModel.cpp
	DSCodeCompletionModel.h
	DSCodeCompletionWorker.cpp
	DSCodeCompletionWorker.h
	DSCodeCompletionContext.cpp
	DSCodeCompletionContext.h
	DSCodeCompletionCodeBody.cpp
	DSCodeCompletionCodeBody.h
	DSCodeCompletionCodeClass.cpp
	DSCodeCompletionCodeClass.h
	items/DSCodeCompletionBaseItem.cpp
	items/DSCodeCompletionBaseItem.h
	items/DSCodeCompletionItem.cpp
	items/DSCodeCompletionItem.h
	items/DSCodeCompletionOverrideFunction.cpp
	items/DSCodeCompletionOverrideFunction.h
	items/DSCCItemFunctionCall.cpp
	items/DSCCItemFunctionCall.h
)

add_library(kdevdscodecompletion STATIC ${codecompletion_SRCS} ${codecompletion_STAT_SRCS})

generate_export_header(kdevdscodecompletion EXPORT_MACRO_NAME KDEVDSCODECOMPLETION_EXPORT EXPORT_FILE_NAME codecompletionexport.h)
target_link_libraries(kdevdscodecompletion LINK_PRIVATE
	KDev::Language
	KDev::Interfaces
	KDev::Project
	kdevdsduchain
	kdevdsparser
)
ecm_qt_declare_logging_category(codecompletion_SRCS
	HEADER parserdebug.h
	IDENTIFIER PARSER
	CATEGORY_NAME "kdevelop.languages.dragonscript.codecompletion"
)

target_include_directories(kdevdscodecompletion
	PUBLIC $<INSTALL_INTERFACE:${KDEVDS_PRIVATE_INCLUDE_DIR}/codecompletion>
	PUBLIC $<INSTALL_INTERFACE:${KDEVDS_PRIVATE_INCLUDE_DIR}>
	PUBLIC $<INSTALL_INTERFACE:${KDEVDS_INCLUDE_DIR}>
)
