/*========================================================
* Implementations.h
* @author Sergey Mikhtonyuk
* @date 09 November 2008
*
* Copyrights (c) Sergey Mikhtonyuk 2007-2010.
* Terms of use, copying, distribution, and modification
* are covered in accompanying LICENSE file
=========================================================*/

#ifndef _IMPLEMENTATIONS_H__
#define _IMPLEMENTATIONS_H__

#include "maps.h"
#include "threadingmodel.h"
#include "allocators.h"

namespace Module
{

	//////////////////////////////////////////////////////////////////////////

	namespace detail
	{
		struct INTERFACE_MAP_ENTRY;

		/// Implementation of IUnknown::QueryInterface method
		/** This method uses interface table, which you define by BEGIN_INTERFACE_MAP macro */
		HResult _QueryInterface(void* pThis, const INTERFACE_MAP_ENTRY* pEntries, SF_RIID riid, void** ppvObject);

		/// Used for chaining QueryInterface
		/** This method used when you have an hierarchy of implementations
		 *  and passing QueryInterface to the top of the tree */
		HResult _Chain(void *pThis, void* pChain, SF_RIID riid, void **ppv);
	}

	//////////////////////////////////////////////////////////////////////////

	/// Inherit this class for IUnknown method implementation
	/** Template parameter defines threading model for operations
	 *  @ingroup Module */
	template<class ThreadingModel = SingleThreadModel>
	class ComRootObject
	{
	public:
		virtual ~ComRootObject() { }

		/// This declaration protects user from calling uuid_of on implementation class
		static const Module::GUID& _get_uuid();

	protected:	
		/// Implementation of IUnknown::AddRef method
		/** This implementation is binded to your class by ComObject class */
		static unsigned long _AddRef(long& refCount) 
		{
			ThreadingModel::Increment(refCount);
			return refCount;
		}

		/// Implementation of IUnknown::Release method
		/** This implementation is binded to your class by ComObject class */
		static unsigned long _Release(long& refCount)
		{
			assert(refCount > 0);
			ThreadingModel::Decrement(refCount);
			return refCount;
		}
	};

	//////////////////////////////////////////////////////////////////////////


	/// ComClassFactory class
	/** Implements IClassFactory methods
	 *  @ingroup Module */
	template <class T>
	class NOVTABLE ComClassFactory : 
		public ComRootObject<>, 
		public IClassFactory
	{
	public:
		/// This class type
		typedef ComClassFactory _ThisClass;

		BEGIN_INTERFACE_MAP()
			INTERFACE_ENTRY(IClassFactory)
		END_INTERFACE_MAP()

		/// Redirects creation to T::CreateInstance()
		HResult CreateInstance(SF_RIID riid, void** ppvObj)
		{
			return T::_CreateInstance(riid, ppvObj);
		}
	};


	//////////////////////////////////////////////////////////////////////////


	/// ComClassFactorySingleton class
	/** Implements IClassFactory methods 
	 *  caching the pointer received after first query
	 *  and returns it for the following queries 
	 * 
	 *  @ingroup Module */
	template <class T>
	class NOVTABLE ComClassFactorySingleton : 
		public ComRootObject<>, 
		public IClassFactory
	{
	public:
		/// This class type
		typedef ComClassFactorySingleton _ThisClass;

		BEGIN_INTERFACE_MAP()
			INTERFACE_ENTRY(IClassFactory)
		END_INTERFACE_MAP()

		/// Ctor
		ComClassFactorySingleton() : pCached(0)
		{
		}

		/// Releases cached pointer
		virtual ~ComClassFactorySingleton()
		{
			if(pCached)
				pCached->Release();
		}

		/// Creates object for first call and then return cached pointer after casting it
		HResult CreateInstance(SF_RIID riid, void** ppvObj)
		{
			if(!pCached)
			{
				HResult hr = T::_CreateInstance(uuid_of(IUnknown), (void**)&pCached);
				if(SF_FAILED(hr))
					return hr;
			}
			return pCached->QueryInterface(riid, ppvObj);
		}

	private:
		/// Cached pointer for singleton object
		IUnknown *pCached;
	};


	//////////////////////////////////////////////////////////////////////////

	namespace detail
	{
		/// ComObject class
		/** Inherits your class and binds IUnknown methods to implementation in ComRootObject
		 *
		 *  @ingroup Module
		 *  @see ComRootObject */
		template<typename Base, template<typename> class Alloc = NewAllocator>
		class ComObject : public Base
		{
		public:

			typedef ComObject<Base, Alloc>	_thisType;
			typedef Alloc<_thisType>		allocator;

			/// Ctor
			ComObject() : mRefCount(0)
			{
			}

			/// Binds ComRootObject impl to your class
			unsigned long AddRef() 
			{
				return Base::_AddRef(mRefCount);
			}

			/// Binds ComRootObject impl to your class
			unsigned long Release() 
			{
				unsigned long t = Base::_Release(mRefCount);
				if(!t)
				{
					allocator alloc;
					alloc.destroy(this);
					alloc.deallocate(this, 1);
				}
				return t;
			}

			/// Used to create instance of implementation class inside the package (do not increases the ref counter)
			static _thisType* _CreateInstance()
			{
				allocator alloc;
				_thisType* p = alloc.allocate(1);
				new(p) _thisType();
				return p;
			}

			/// Creates and checks for required interface
			static HResult _CreateInstance(SF_RIID riid, void** ppv)
			{
				if (ppv == 0) return SF_E_POINTER;
				*ppv = 0;

				HResult hRes = SF_E_OUTOFMEMORY;
				_thisType* p = 0;

				p = _CreateInstance();

				if (p != 0)
				{
					p->AddRef();
					hRes = p->QueryInterface(riid, ppv);
					p->Release();
				}
				return hRes;
			}


		private:
			/// Reference counter
			long mRefCount;
		};

		//////////////////////////////////////////////////////////////////////////

	} // detail

} // namespace

//////////////////////////////////////////////////////////////////////////


/// Creates implementation class instance (refCount == 1)
/** @ingroup Module */
template<class Impl>
Module::HResult create_instance_impl(Impl** pp)
{
	if(!pp) return SF_E_POINTER;
	*pp = 0;

	typename Impl::_ObjectClass * pImpl = Impl::_ObjectClass::_CreateInstance();
	if(!pImpl) return SF_E_FAIL;

	pImpl->AddRef();
	*pp = pImpl;
	return SF_S_OK;
}

//////////////////////////////////////////////////////////////////////////


/// Creates implementation and checks for specified interface (refCount == 1)
/** @ingroup Module */
template<class Impl, class Itf>
Module::HResult create_instance(Itf** pp)
{
	// NOTE: if you get an ambiguity error here
	// then you have tried to call create_instance to
	// create implementation class - use create_instance_impl instead
	return Impl::_ObjectClass::_CreateInstance(UUID_PPV(Itf, pp));
}

/// Creates implementation and checks for specified interface (refCount == 1)
/** @ingroup Module */
template<class Impl>
Module::HResult create_instance(void** pp, SF_RIID iid)
{
	return Impl::_ObjectClass::_CreateInstance(iid, pp);
}

//////////////////////////////////////////////////////////////////////////


#endif // _IMPLEMENTATIONS_H__
