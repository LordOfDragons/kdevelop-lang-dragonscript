#ifndef DSPROJECTCONFIGPAGE_H
#define DSPROJECTCONFIGPAGE_H

#include <QStringListModel>
#include <project/projectconfigpage.h>

#include "configpageexport.h"
#include "../DSProjectSettings.h"

using namespace KDevelop;

class Ui_ProjectConfig;

namespace DragonScript {

class KDEVDSCONFIGPAGE_EXPORT ProjectConfigPage : public ConfigPage{
	Q_OBJECT
	
private:
	IProject *pProject;
	DSProjectSettings pSettings;
	Ui_ProjectConfig *pUI;
	QStringListModel *pModelPathInclude;
	
public:
	ProjectConfigPage( IPlugin *self, const KDevelop::ProjectConfigOptions &options, QWidget *parent );
	~ProjectConfigPage();
	
	QString name() const override;
	
public Q_SLOTS:
	void apply() override;
	void defaults() override;
	void reset() override;
	
	void btnPathIncludeSelect();
	void btnPathIncludeAdd();
	void btnPathIncludeRemove();
	
	void chkRequireDragenginePackageChanged();
};

}

#endif
