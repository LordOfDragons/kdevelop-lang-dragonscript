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
}

Namespace::Namespace( Namespace &parent, const IndexedIdentifier &identifier ) :
pParent( &parent ),
pIdentifier( identifier ),
pQualifiedIdentifier( parent.pQualifiedIdentifier.identifier() + identifier ),
pDirtyContent( true ){
}



const Namespace::TypeNamespaceMap &Namespace::namespaces(){
	if( pDirtyContent ){
		findContent();
	}
	return pNamespaces;
}

const Namespace::TypeClassMap &Namespace::classes(){
	if( pDirtyContent ){
		findContent();
	}
	return pClasses;
}

Namespace *Namespace::getNamespace( const IndexedIdentifier &iid ){
	if( pDirtyContent ){
		findContent();
	}
	
	const TypeNamespaceMap::const_iterator iter( pNamespaces.constFind( iid ) );
	
	if( iter != pNamespaces.cend() ){
		return iter.value().data();
		
	}else{
		return nullptr;
	}
}

Namespace *Namespace::getNamespace( const QString &name ){
	return getNamespace( IndexedIdentifier( Identifier( name ) ) );
}

Namespace *Namespace::getNamespace( const QualifiedIdentifier &qid ){
	const int count = qid.count();
	if( count == 0 ){
		return nullptr;
	}
	
	Namespace *ns = getNamespace( IndexedIdentifier( qid.first() ) );
	if( ! ns ){
		return nullptr;
	}
	
	int i;
	for( i=1; i<count; i++ ){
		ns = ns->getNamespace( IndexedIdentifier( qid.at( i ) ) );
		if( ! ns ){
			return nullptr;
		}
	}
	
	return ns;
}

Namespace &Namespace::getOrAddNamespace( const IndexedIdentifier &iid ){
	Namespace *ns = getNamespace( iid );
	
	if( ! ns ){
		ns = new Namespace( *this, iid );
		pNamespaces.insert( iid, Ref( ns ) );
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
	pDirtyContent = false;
	foreach( const DUContextPointer &context, pContexts ){
		if( ! context ){
			continue;
		}
		const QVector<Declaration*> declarations( context->localDeclarations() );
		foreach( Declaration *decl, declarations ){
			if( decl->kind() == Declaration::Type ){
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
					pNamespaces.insert( identifier, Ref( ns ) );
				}
				
				DUContext * const ictx = decl->internalContext();
				if( ictx ){
					ns->pContexts << DUContextPointer( ictx );
				}
				
				if( ! ns->pDeclaration ){
					ns->pDeclaration = dynamic_cast<ClassDeclaration*>( decl );
				}
			}
		}
	}
}

}
