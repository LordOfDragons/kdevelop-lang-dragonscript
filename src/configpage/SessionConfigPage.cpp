#include <QFileDialog>
#include <interfaces/icore.h>
#include <interfaces/isession.h>

#include "SessionConfigPage.h"
#include "ui_sessionConfigPage.h"
#include "../DSSessionSettings.h"


namespace DragonScript {

SessionConfigPage::SessionConfigPage( IPlugin* self, QWidget *parent ) :
ConfigPage( self, nullptr, parent ),
pUI( new Ui_SessionConfig )
{
	pConfigGroup = ICore::self()->activeSession()->config()->group( "dragonscriptsupport" );
	pUI->setupUi( this );
	
	connect( pUI->editPathDSI, SIGNAL(textChanged(const QString &)), this, SLOT(editPathDSIChanged()) );
	connect( pUI->btnPathDSI, SIGNAL(clicked()), this, SLOT(btnPathDSISelect()) );
	connect( pUI->editPathDragengine, SIGNAL(textChanged(const QString &)), this, SLOT(editPathDragengineChanged()) );
	connect( pUI->btnPathDragengine, SIGNAL(clicked()), this, SLOT(btnPathDragengineSelect()) );
}

SessionConfigPage::~SessionConfigPage(){
}

QString SessionConfigPage::name() const{
	return i18n("DragonScript Language");
}

QString SessionConfigPage::fullName() const{
	return i18n("Configure DragonScript Language");
}

QIcon SessionConfigPage::icon() const{
	return QIcon::fromTheme( QStringLiteral( "kdevelop" ) );
}

KDevelop::ConfigPage::ConfigPageType SessionConfigPage::configPageType() const{
	return ConfigPage::LanguageConfigPage;
}



void SessionConfigPage::apply(){
	pConfigGroup.writeEntry( "pathDSI", pUI->editPathDSI->text() );
	pConfigGroup.writeEntry( "pathDragengine", pUI->editPathDragengine->text() );
	
	DSSessionSettings::self.update();
}

void SessionConfigPage::reset(){
	pUI->editPathDSI->setText( pConfigGroup.readEntry( "pathDSI", "" ) );
	pUI->editPathDragengine->setText( pConfigGroup.readEntry( "pathDragengine", "" ) );
	
	emit changed();
}



void SessionConfigPage::editPathDSIChanged(){
	emit changed();
}

void SessionConfigPage::btnPathDSISelect(){
	QString path( pUI->editPathDSI->text() );
	if( path.isEmpty() ){
		path = "";
	}
	
	path = QFileDialog::getOpenFileName( this, i18n("Select DSI Binary Path"),
		path, QString(), nullptr, QFileDialog::DontResolveSymlinks );
	
	if( ! path.isEmpty() ){
		pUI->editPathDSI->setText( path );
		emit changed();
	}
}

void SessionConfigPage::editPathDragengineChanged(){
	emit changed();
}

void SessionConfigPage::btnPathDragengineSelect(){
	QString path( pUI->editPathDragengine->text() );
	if( path.isEmpty() ){
		path = "";
	}
	
	path = QFileDialog::getExistingDirectory( this, i18n("Select Drag[en]gine Module Directory"),
		path, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks );
	
	if( ! path.isEmpty() ){
		pUI->editPathDragengine->setText( path );
		emit changed();
	}
}

}
