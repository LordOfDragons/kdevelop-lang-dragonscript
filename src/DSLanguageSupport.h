#ifndef DSLANGUAGESUPPORT_H
#define DSLANGUAGESUPPORT_H

#include <interfaces/idocument.h>
#include <interfaces/iplugin.h>
#include <interfaces/ilanguagecheck.h>
#include <interfaces/ilanguagecheckprovider.h>
#include <language/interfaces/ilanguagesupport.h>
#include <language/duchain/topducontext.h>

#include "duchain/ImportPackages.h"


using namespace KDevelop;

namespace DragonScript {

class Highlighting;

/**
 * Language support module for DragonScript language.
 */
class DSLanguageSupport : public IPlugin, public ILanguageSupport {
	Q_OBJECT
	Q_INTERFACES(KDevelop::ILanguageSupport)
	
private:
	Highlighting *pHighlighting;
	static DSLanguageSupport *pSelf;
	
	ImportPackages pImportPackages;
	
	
	
public:
	/** Create language support. */
	DSLanguageSupport(QObject* parent, const QVariantList& args);
	
	/** Clean up language support. */
	virtual ~DSLanguageSupport();
	
	
	
	/** Singleton. */
	inline static DSLanguageSupport *self(){ return pSelf; }
	
	/** Name of language.*/
	QString name() const override;
	
	/** Create parse job used by background parser to parse \p url.*/
	ParseJob *createParseJob( const IndexedString &url ) override;
	
	/** Get the number of available config pages for global settings. */
	int configPages() const override;
	
	/** Get the global config page */
	ConfigPage *configPage( int number, QWidget *parent ) override;
	
	/** Code highlighting instance. */
	ICodeHighlighting *codeHighlighting() const override;
	
	/** Per-project configuration pages. */
	int perProjectConfigPages() const override;
	
	/** Create per-project configuration page. */
	ConfigPage* perProjectConfigPage( int number, const KDevelop::ProjectConfigOptions &options,
		QWidget *parent ) override;
	
	/** Import packages. */
	inline ImportPackages &importPackages(){ return pImportPackages; }
};

}

#endif
