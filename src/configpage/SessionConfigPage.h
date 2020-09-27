#ifndef DSSESSIONCONFIGPAGE_H
#define DSSESSIONCONFIGPAGE_H

#include <QStringListModel>
#include <KConfigGroup>
#include <project/projectconfigpage.h>

#include "configpageexport.h"

using namespace KDevelop;

class Ui_SessionConfig;

namespace DragonScript {

class KDEVDSCONFIGPAGE_EXPORT SessionConfigPage : public ConfigPage{
	Q_OBJECT
	
private:
	KConfigGroup pConfigGroup;
	Ui_SessionConfig *pUI;
	
public:
	SessionConfigPage( IPlugin *self, QWidget *parent );
	~SessionConfigPage();
	
	QString name() const override;
	QString fullName() const override;
	QIcon icon() const override;
	
	KDevelop::ConfigPage::ConfigPageType configPageType() const override;
	
public Q_SLOTS:
	void apply() override;
	void reset() override;
	
	void editPathDSIChanged();
	void btnPathDSISelect();
	void editPathDragengineChanged();
	void btnPathDragengineSelect();
};

}

#endif
