#include <QDebug>

#include <language/duchain/types/structuretype.h>

#include "TypeFinder.h"
#include "ImportPackage.h"
#include "ImportPackageLanguage.h"


using namespace KDevelop;

namespace DragonScript {

TypeFinder::~TypeFinder(){
	DUChainWriteLocker lock;
	pSearchContexts.clear();
}

ClassDeclaration *TypeFinder::typeByte(){
	if( ! pDeclByte ){
		const IndexedIdentifier identifier( (Identifier( "byte" )) );
		pDeclByte = declarationForIntegral( identifier );
		pIdentifierMap.insert( identifier, pDeclByte );
	}
	return pDeclByte.data();
}

ClassDeclaration *TypeFinder::typeBool(){
	if( ! pDeclBool ){
		const IndexedIdentifier identifier( (Identifier( "bool" )) );
		pDeclBool = declarationForIntegral( identifier );
		pIdentifierMap.insert( identifier, pDeclBool );
	}
	return pDeclBool.data();
}

ClassDeclaration *TypeFinder::typeInt(){
	if( ! pDeclInt ){
		const IndexedIdentifier identifier( (Identifier( "int" )) );
		pDeclInt = declarationForIntegral( identifier );
		pIdentifierMap.insert( identifier, pDeclInt );
	}
	return pDeclInt.data();
}

ClassDeclaration *TypeFinder::typeFloat(){
	if( ! pDeclFloat ){
		const IndexedIdentifier identifier( (Identifier( "float" )) );
		pDeclFloat = declarationForIntegral( identifier );
		pIdentifierMap.insert( identifier, pDeclFloat );
	}
	return pDeclFloat.data();
}

ClassDeclaration *TypeFinder::typeString(){
	if( ! pDeclString ){
		const IndexedIdentifier identifier( (Identifier( "String" )) );
		pDeclString = declarationForIntegral( identifier );
		pIdentifierMap.insert( identifier, pDeclString );
	}
	return pDeclString.data();
}

ClassDeclaration *TypeFinder::typeObject(){
	if( ! pDeclObject ){
		const IndexedIdentifier identifier( (Identifier( "Object" )) );
		pDeclObject = declarationForIntegral( identifier );
		pIdentifierMap.insert( identifier, pDeclObject );
	}
	return pDeclObject.data();
}

ClassDeclaration *TypeFinder::typeBlock(){
	if( ! pDeclBlock ){
		const IndexedIdentifier identifier( (Identifier( "Block" )) );
		pDeclBlock = declarationForIntegral( identifier );
		pIdentifierMap.insert( identifier, pDeclBlock );
	}
	return pDeclBlock.data();
}

ClassDeclaration *TypeFinder::typeEnumeration(){
	if( ! pDeclEnumeration ){
		const IndexedIdentifier identifier( (Identifier( "Enumeration" )) );
		pDeclEnumeration = declarationForIntegral( identifier );
		pIdentifierMap.insert( identifier, pDeclEnumeration );
	}
	return pDeclEnumeration.data();
}



ClassDeclaration *TypeFinder::declarationFor( const AbstractType::Ptr &type ){
	TypeMap::const_iterator iter( pTypeMap.constFind( type ) );
	if( iter != pTypeMap.constEnd() /*&& *iter */){
		return iter->data();
	}
	
	const StructureType::Ptr structType( type.cast<StructureType>() );
	if( ! structType ){
		pTypeMap.insert( type, {} ); // avoid looking up the type again
		return nullptr;
	}
	
	DUChainReadLocker lock;
	foreach( const ReferencedTopDUContext &top, pSearchContexts ){
		if( ! top ){
			continue;
		}
		
		Declaration * const decl = structType->declaration( top.data() );
		if( ! decl ){
			continue;
		}
		
		ClassDeclaration * const classDecl = dynamic_cast<ClassDeclaration*>( decl );
		if( ! classDecl ){
			continue;
		}
		
		pTypeMap.insert( type, ClassDeclarationPointer( classDecl ) );
		return classDecl;
	}
	
	pTypeMap.insert( type, {} ); // avoid looking up the type again
	return nullptr;
}

DUContext *TypeFinder::contextFor( const AbstractType::Ptr &type ){
	const ClassDeclaration * const d = declarationFor( type );
	return d ? d->internalContext() : nullptr;
}

ClassDeclaration *TypeFinder::declarationFor( const IndexedIdentifier &identifier ){
	IdentifierMap::const_iterator iter( pIdentifierMap.constFind( identifier ) );
	if( iter != pIdentifierMap.constEnd() /*&& *iter*/ ){
		return iter->data();
	}
	
	DUChainReadLocker lock;
	foreach( const ReferencedTopDUContext &top, pSearchContexts ){
		if( ! top ){
			continue;
		}
		
		const QList<Declaration*> declarations( top->findLocalDeclarations( identifier ) );
		if( declarations.isEmpty() ){
			continue;
		}
		
		ClassDeclaration * const classDecl = dynamic_cast<ClassDeclaration*>( declarations.last() );
		if( ! classDecl ){
			continue;
		}
		
		pIdentifierMap.insert( identifier, ClassDeclarationPointer( classDecl ) );
		return classDecl;
	}
	
	pIdentifierMap.insert( identifier, {} ); // avoid looking up the identifier again
	return nullptr;
}



ClassDeclaration *TypeFinder::declarationForIntegral( const IndexedIdentifier &identifier ){
	DUChainReadLocker lock;
	
	if( pLangClasses.isEmpty() ){
		ImportPackage::State state;
		ImportPackageLanguage::self()->contexts( state );
		if( state.ready ){
			foreach( TopDUContext *each, state.importContexts ){
				pLangClasses << TopDUContextPointer( each );
			}
		}
	}
	
	foreach( const TopDUContextPointer &context, pLangClasses ){
		const QList<Declaration*> found( context->findLocalDeclarations( identifier ) );
		if( ! found.isEmpty() ){
			return static_cast<ClassDeclaration*>( found.first() );
		}
	}
	
	return nullptr;
}

}
