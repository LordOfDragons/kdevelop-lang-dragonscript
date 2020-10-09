#include <QDebug>

#include <language/duchain/types/structuretype.h>
#include <language/duchain/topducontext.h>

#include "Namespace.h"
#include "TypeFinder.h"
#include "Helpers.h"


using namespace KDevelop;

namespace DragonScript {

Namespace::Namespace( TypeFinder &typeFinder ) :
pParent( nullptr ),
pDirtyContent( true )
{
	foreach( const ReferencedTopDUContext &each, typeFinder.searchContexts() ){
		pContexts << DUContextPointer( each.data() );
	}
	qDebug() << "constructor1" << pParent << this << pIdentifier.identifier();
}

Namespace::Namespace( Namespace &parent, const IndexedIdentifier &identifier ) :
pParent( &parent ),
pIdentifier( identifier ),
pDirtyContent( true ){
	qDebug() << "constructor2" << pParent << this << pIdentifier.identifier();
}

Namespace::~Namespace(){
	qDebug() << "destructor" << pParent << this << pIdentifier.identifier();
}



Namespace *Namespace::getNamespace( const IndexedIdentifier &iid ){
	if( pDirtyContent ){
		findContent();
	}
	
	const TypeNamespaceMap::const_iterator iter( pNamespaces.constFind( iid ) );
	
	if( iter != pNamespaces.cend() ){
		qDebug() << "found namespace" << iter.value().data() << iter.value().data()->pIdentifier << iter.value().data()->pParent;
		return iter.value().data();
		
	}else{
		return nullptr;
	}
}

Namespace &Namespace::getOrAddNamespace( const IndexedIdentifier &iid ){
	Namespace *ns = getNamespace( iid );
	
	if( ! ns ){
		ns = new Namespace( *this, iid );
		pNamespaces.insert( iid, Ref( ns ) );
		qDebug() << "create namespace" << ns << ns->pIdentifier << ns->pParent << pNamespaces.count();
	}
	
	return *ns;
}

Namespace &Namespace::getOrAddNamespace( const QString &name ){
	return getOrAddNamespace( IndexedIdentifier( Identifier( name ) ) );
}

ClassDeclaration *Namespace::getClass( const IndexedIdentifier &iid ){
	if( pDirtyContent ){
		findContent();
	}
	
	const TypeClassMap::const_iterator iter( pClasses.constFind( iid ) );
	
	if( iter == pClasses.cend() ){
		return nullptr;
		
	}else{
		return iter.value().data();
	}
}



void Namespace::findContent(){
			qDebug() << "findContent" << pParent << pIdentifier.identifier();
	pDirtyContent = false;
	foreach( const DUContextPointer &context, pContexts ){
		if( ! context ){
			continue;
		}
// 				qDebug() << "context" << context->scopeIdentifier(true);
		const QVector<Declaration*> declarations( context->localDeclarations() );
		foreach( Declaration *decl, declarations ){
			if( decl->kind() == Declaration::Type ){
// 					qDebug() << "decl class" << decl->toString();
				ClassDeclaration * const classDecl = dynamic_cast<ClassDeclaration*>( decl );
				if( ! classDecl ){
					continue;
				}
				
				const IndexedIdentifier &identifier = classDecl->indexedIdentifier();
				if( pClasses.contains( identifier ) ){
					// duplicate class is a bug. stick to the first found one
					continue;
				}
				
				pClasses.insert( identifier, ClassDeclarationPointer( classDecl ) );
				
			}else if( decl->kind() == Declaration::Namespace ){
// 					qDebug() << "decl namespace" << decl->toString();
				if( ! pDeclaration ){
					pDeclaration = dynamic_cast<ClassDeclaration*>( context->owner() );
				}
				
				const IndexedIdentifier &identifier = decl->indexedIdentifier();
				if( pClasses.contains( identifier ) ){
					// namespace with same name as a class is a bug. stick to the class
					continue;
				}
				
				const TypeNamespaceMap::const_iterator iter( pNamespaces.constFind( identifier ) );
				Namespace *ns;
				
				if( iter != pNamespaces.cend() ){
					ns = iter.value().data();
					
				}else{
					ns = new Namespace( *this, identifier );
					qDebug() << "add namespace" << this << pIdentifier << identifier << ns;
					pNamespaces.insert( identifier, Ref( ns ) );
					qDebug() << "count now" << pNamespaces.count();
				}
				
				DUContext * const ictx = decl->internalContext();
				if( ictx ){
// 					qDebug() << "add context" << this << ictx->localScopeIdentifier() << ictx;
					ns->pContexts << DUContextPointer( ictx );
				}
			}
		}
	}
	qDebug() << "exit findContent" << pParent << pIdentifier.identifier();
}

}
