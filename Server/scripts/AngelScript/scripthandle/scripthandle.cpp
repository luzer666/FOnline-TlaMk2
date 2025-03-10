#include "scripthandle.h"
#include <assert.h>

BEGIN_AS_NAMESPACE

static void Construct(CScriptHandle *self) { new(self) CScriptHandle(); }
static void Construct(CScriptHandle *self, const CScriptHandle &o) { new(self) CScriptHandle(o); }
// This one is not static because it needs to be friend with the CScriptHandle class
void Construct(CScriptHandle *self, void *ref, int typeId) { new(self) CScriptHandle(ref, typeId); }
static void Destruct(CScriptHandle *self) { self->~CScriptHandle(); }

CScriptHandle::CScriptHandle()
{
	m_ref  = 0;
	m_type = 0;
}

CScriptHandle::CScriptHandle(const CScriptHandle &other)
{
	m_ref  = other.m_ref;
	m_type = other.m_type;

	AddRefHandle();
}

CScriptHandle::CScriptHandle(void *ref, asITypeInfo *type)
{
	m_ref  = ref;
	m_type = type;

	AddRefHandle();
}

// This constructor shouldn't be called from the application 
// directly as it requires an active script context
CScriptHandle::CScriptHandle(void *ref, int typeId)
{
	m_ref  = 0;
	m_type = 0;

	Assign(ref, typeId);
}

CScriptHandle::~CScriptHandle()
{
	ReleaseHandle();
}

void CScriptHandle::ReleaseHandle()
{
	if( m_ref && m_type )
	{
		ASEngine->ReleaseScriptObject(m_ref, m_type);

		m_ref  = 0;
		m_type = 0;
	}
}

void CScriptHandle::AddRefHandle()
{
	if( m_ref && m_type )
		ASEngine->AddRefScriptObject(m_ref, m_type);
}

CScriptHandle &CScriptHandle::operator =(const CScriptHandle &other)
{
	// Don't do anything if it is the same reference
	if( m_ref == other.m_ref )
		return *this;

	ReleaseHandle();

	m_ref  = other.m_ref;
	m_type = other.m_type;

	AddRefHandle();

	return *this;
}

void CScriptHandle::Set(void *ref, asITypeInfo *type)
{
	if( m_ref == ref ) return;

	ReleaseHandle();

	m_ref  = ref;
	m_type = type;

	AddRefHandle();
}

asITypeInfo *CScriptHandle::GetType()
{
	return m_type;
}

// This method shouldn't be called from the application 
// directly as it requires an active script context
CScriptHandle &CScriptHandle::Assign(void *ref, int typeId)
{
	// When receiving a null handle we just clear our memory
	if( typeId == 0 )
	{
		Set(0, 0);
		return *this;
	}

	// Dereference received handles to get the object
	if( typeId & asTYPEID_OBJHANDLE )
	{
		Log( "Handle\n" );
		// Store the actual reference
		ref = *(void**)ref;
		typeId &= ~asTYPEID_OBJHANDLE;
	}
	else
		Log( "No handle\n" );

	// Get the object type
	asIScriptContext *ctx    = ScriptGetActiveContext();
	asITypeInfo    *type   = ASEngine->GetTypeInfoById(typeId);

	Set(ref, type);

	return *this;
}

bool CScriptHandle::operator==(const CScriptHandle &o) const
{
	if( m_ref  == o.m_ref &&
		m_type == o.m_type )
		return true;

	// TODO: If type is not the same, we should attempt to do a dynamic cast,
	//       which may change the pointer for application registered classes

	return false;
}

bool CScriptHandle::operator!=(const CScriptHandle &o) const
{
	return !(*this == o);
}

bool CScriptHandle::Equals(void *ref, int typeId) const
{
	// Null handles are received as reference to a null handle
	if( typeId == 0 )
		ref = 0;

	// Dereference handles to get the object
	if( typeId & asTYPEID_OBJHANDLE )
	{
		// Compare the actual reference
		ref = *(void**)ref;
		typeId &= ~asTYPEID_OBJHANDLE;
	}

	// TODO: If typeId is not the same, we should attempt to do a dynamic cast, 
	//       which may change the pointer for application registered classes

	if( ref == m_ref ) return true;

	return false;
}

// AngelScript: used as '@obj = cast<obj>(ref);'
void CScriptHandle::Cast(void **outRef, int typeId)
{
	// If we hold a null handle, then just return null
	if( m_type == 0 )
	{
		*outRef = 0;
		return;
	}
	
	// It is expected that the outRef is always a handle
	assert( typeId & asTYPEID_OBJHANDLE );

	// Compare the type id of the actual object
	typeId &= ~asTYPEID_OBJHANDLE;
	asITypeInfo    *type   = ASEngine->GetTypeInfoById(typeId);

	*outRef = 0;

	if( type == m_type )
	{
		// If the requested type is a script function it is 
		// necessary to check if the functions are compatible too
		if( m_type->GetFlags() & asOBJ_SCRIPT_FUNCTION )
		{
			asIScriptFunction *func = reinterpret_cast<asIScriptFunction*>(m_ref);
			if( !func->IsCompatibleWithTypeId(typeId) )
				return;
		}

		// Must increase the ref count as we're returning a new reference to the object
		AddRefHandle();
		*outRef = m_ref;
	}
	else if( m_type->GetFlags() & asOBJ_SCRIPT_OBJECT )
	{
		// Attempt a dynamic cast of the stored handle to the requested handle type
		if( ASEngine->IsHandleCompatibleWithObject(m_ref, m_type->GetTypeId(), typeId) )
		{
			// The script type is compatible so we can simply return the same pointer
			AddRefHandle();
			*outRef = m_ref;
		}
	}
	else
	{
		// TODO: Check for the existance of a reference cast behaviour. 
		//       Both implicit and explicit casts may be used
		//       Calling the reference cast behaviour may change the actual 
		//       pointer so the AddRef must be called on the new pointer
	}
}

ScriptString* CScriptHandle::script_TypeName()
{
	if( m_ref && m_type )
		return &ScriptString::Create( m_type->GetName( ) );
	else return &ScriptString::Create( "null" );
}

ScriptString* CScriptHandle::script_TypeNamespace()
{
	if( m_ref && m_type )
		return &ScriptString::Create( m_type->GetNamespace( ) );
	else return &ScriptString::Create( "" );
}

CScriptHandle* CreateScriptHandle( ScriptString* typeName )
{
	if( !typeName )
		return NULL;
	
	{
		asIScriptModule *module = ASEngine->GetModule( "Mk2" );		
		if( module )
		{
			int typeId = module->GetTypeIdByDecl( typeName->c_str() );
			if( typeId > 0 )
			{
				asITypeInfo *type = ASEngine->GetTypeInfoByName( typeName->c_str() );
				if( type )
				{
					string factoryName = typeName->c_str();
					factoryName += " @";
					factoryName += typeName->c_str();
					factoryName += "()";
					
					asIScriptObject *obj = reinterpret_cast<asIScriptObject*>(ASEngine->CreateScriptObject(type));
					if( obj )
					{
						CScriptHandle* ref = new CScriptHandle( );
						if( ref )
							ref->Assign( obj, typeId );
						obj->Release();
						return ref;
					}
				}
			}
		}
	}
	return NULL;
}

void CScriptHandle::AddRef(){ RefCount++; }
void CScriptHandle::Release(){ if( --RefCount == 0 ) Destruct( this ); }

CScriptHandle* CreateCScriptHandle() 						{ return new CScriptHandle(); }
CScriptHandle* CopyScriptHandle(const CScriptHandle &o) 		{ return new CScriptHandle(o); }
CScriptHandle* FormatCScriptHandle(void *ref, int typeId) 	{ return new CScriptHandle(ref, typeId); }

void RegisterScriptHandle_Native()
{
	int r;
	r = ASEngine->RegisterObjectType( "Object", sizeof( CScriptHandle ), asOBJ_REF ); assert( r >= 0 );
	r = ASEngine->RegisterObjectBehaviour( "Object", asBEHAVE_ADDREF, "void f()", asMETHOD( CScriptHandle, AddRef ), asCALL_THISCALL ); assert( r >= 0 );
    r = ASEngine->RegisterObjectBehaviour( "Object", asBEHAVE_RELEASE, "void f()", asMETHOD( CScriptHandle, Release ), asCALL_THISCALL ); assert( r >= 0 );
	r = ASEngine->RegisterObjectBehaviour( "Object", asBEHAVE_REF_CAST, "void f(?&out)", asMETHODPR(CScriptHandle, Cast, (void **, int), void), asCALL_THISCALL); assert( r >= 0 );
	
	r = ASEngine->RegisterObjectMethod( "Object", "Object &opAssign(const Object &in)", asMETHOD(CScriptHandle, operator=), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Object", "Object &opAssign(const ?&in)", asMETHOD(CScriptHandle, Assign), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Object", "bool opEquals(const Object &in) const", asMETHODPR(CScriptHandle, operator==, (const CScriptHandle &) const, bool), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Object", "bool opEquals(const ?&in) const", asMETHODPR(CScriptHandle, Equals, (void*, int) const, bool), asCALL_THISCALL); assert( r >= 0 );
	
	// Type information
	r = ASEngine->RegisterObjectMethod( "Object", "string@ get_TypeName( ) const", asMETHOD(CScriptHandle, script_TypeName), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Object", "string@ get_TypeNamespace( ) const", asMETHOD(CScriptHandle, script_TypeNamespace), asCALL_THISCALL); assert( r >= 0 );

	const char* defName = ASEngine->GetDefaultNamespace();
    ASEngine->SetDefaultNamespace( "Object" );
	r = ASEngine->RegisterGlobalFunction( "::Object@+ Create( )", asFUNCTION( CreateCScriptHandle ), asCALL_CDECL ); assert( r >= 0 );
	r = ASEngine->RegisterGlobalFunction( "::Object@+ Create( const ::Object &in )", asFUNCTION( CopyScriptHandle ), asCALL_CDECL ); assert( r >= 0 );
	r = ASEngine->RegisterGlobalFunction( "::Object@+ Create( const ?&in )", asFUNCTION( FormatCScriptHandle ), asCALL_CDECL ); assert( r >= 0 );
	r = ASEngine->RegisterGlobalFunction( "::Object@+ Create( ::string@ type )", asFUNCTION( CreateScriptHandle ), asCALL_CDECL ); assert( r >= 0 );
    ASEngine->SetDefaultNamespace( defName );
}

void RegisterScriptHandle()
{
	RegisterScriptHandle_Native();
}


END_AS_NAMESPACE
