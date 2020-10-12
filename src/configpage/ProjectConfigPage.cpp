#include <QFileDialog>

#include <interfaces/icore.h>
#include <interfaces/ilanguagecontroller.h>
#include <language/backgroundparser/backgroundparser.h>

#include "ProjectConfigPage.h"
#include "ui_projectConfigPage.h"
#include "../DSParseJob.h"
#include "../duchain/Helpers.h"


using namespace KDevelop;

namespace DragonScript {

ProjectConfigPage::ProjectConfigPage( IPlugin* self, const ProjectConfigOptions &options, QWidget *parent ) :
ConfigPage( self, nullptr, parent ),
pProject( options.project ),
pUI( new Ui_ProjectConfig )
{
	pSettings.load( *options.project );
	pUI->setupUi( this );
	
	pModelPathInclude = new QStringListModel( this );
	pUI->listPathInclude->setModel( pModelPathInclude );
	
	connect( pUI->btnPathIncludeSelect, SIGNAL(clicked()), this, SLOT(btnPathIncludeSelect()) );
	connect( pUI->btnPathIncludeAdd, SIGNAL(clicked()), this, SLOT(btnPathIncludeAdd()) );
	connect( pUI->btnPathIncludeRemove, SIGNAL(clicked()), this, SLOT(btnPathIncludeRemove()) );
}

ProjectConfigPage::~ProjectConfigPage(){
}

QString ProjectConfigPage::name() const{
	return i18n( "DragonScript Settings" );
}



void ProjectConfigPage::apply(){
	pSettings.setPathInclude( pModelPathInclude->stringList() );
	pSettings.setRequireDragenginePackage( pUI->chkRequireDragenginePackage->isChecked() );
	pSettings.save( *pProject );
	
	// force reparsing
	BackgroundParser &bp = *ICore::self()->languageController()->backgroundParser();
	
	const QSet<IndexedString> files( pProject->fileSet() );
	foreach( const IndexedString &file, files ){
		if( file.str().endsWith( "*.ds" ) ){
			bp.addDocument( file, TopDUContext::ForceUpdate );
		}
	}
}

void ProjectConfigPage::defaults(){
	pUI->editPathInclude->setText( "" );
	pModelPathInclude->removeRows( 0, pModelPathInclude->rowCount() );
	
	pUI->chkRequireDragenginePackage->setChecked( false );
	
	emit changed();
}

void ProjectConfigPage::reset(){
	pModelPathInclude->setStringList( pSettings.pathInclude() );
	pUI->chkRequireDragenginePackage->setChecked( pSettings.requireDragenginePackage() );
	
	emit changed();
}



void ProjectConfigPage::btnPathIncludeSelect(){
	QString path( pUI->editPathInclude->text() );
	if( path.isEmpty() ){
		path = pProject->path().toLocalFile();
	}
	
	path = QFileDialog::getExistingDirectory( this, i18n("Select Include Directory"),
		path, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks );
	
	if( ! path.isEmpty() ){
		pUI->editPathInclude->setText( path );
	}
}

void ProjectConfigPage::btnPathIncludeAdd(){
	const QString path( pUI->editPathInclude->text() );
	if( path.isEmpty() ){
		return;
	}
	
	if( pModelPathInclude->stringList().contains( path ) ){
		return;
	}
	
	const int row = pModelPathInclude->rowCount();
	pModelPathInclude->insertRows( row, 1 );
	const QModelIndex index( pModelPathInclude->index( row ) );
	pModelPathInclude->setData( index, path );
	pUI->listPathInclude->setCurrentIndex( index );
	
	emit changed();
}

void ProjectConfigPage::btnPathIncludeRemove(){
	const QModelIndex index( pUI->listPathInclude->currentIndex() );
	if( ! index.isValid() ){
		return;
	}
	
	pModelPathInclude->removeRows( index.row(), 1 );
	
	emit changed();
}

void ProjectConfigPage::chkRequireDragenginePackageChanged(){
}

}
