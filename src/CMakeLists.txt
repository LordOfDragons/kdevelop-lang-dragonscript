cmake_policy(SET CMP0071 NEW)

set(DESHAREDIR "/usr/share/dragengine" CACHE STRING "Drag[en]gine Share Directory")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

add_definitions(-DDESHAREDIR="${DESHAREDIR}")

add_subdirectory(parser)
add_subdirectory(duchain)
add_subdirectory(codecompletion)
add_subdirectory(configpage)

kdevplatform_add_plugin(dragonscriptlanguagesupport
	JSON kdev_lang_dscript.json
	SOURCES DSLanguageSupport.cpp
	        DSParseJob.cpp
	        Highlighting.cpp
	        DSSessionSettings.cpp
	        DSProjectSettings.cpp
)

target_link_libraries(dragonscriptlanguagesupport
	KDev::Interfaces
	KDev::Language
	KF5::ThreadWeaver
	KF5::TextEditor
	kdevdsparser
	kdevdsduchain
	kdevdscodecompletion
	kdevdsconfigpage
)

#KDev::Util

install(FILES org.kde.kdev-dragonscript.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(DIRECTORY dslangdoc DESTINATION ${KDE_INSTALL_DATADIR}/kdevdragonscriptsupport)

install(FILES kate-dragonscript.xml DESTINATION ${KDE_INSTALL_DATADIR}/katepart5/syntax)

install(FILES dragonscript-mimetype.xml DESTINATION ${KDE_INSTALL_MIMEDIR})
update_xdg_mimetypes(${KDE_INSTALL_MIMEDIR})
