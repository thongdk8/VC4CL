/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_OBJECT_H
#define VC4CL_OBJECT_H

#include "ObjectTracker.h"
#include "common.h"

#include <CL/opencl.h>

#include <cstring>
#include <string>

namespace vc4cl
{
	//This class exists only, so the object-tracker can track Objects on a common base class
	class BaseObject
	{
	public:
		BaseObject(std::string typeName) : typeName(std::move(typeName)), referenceCount(1)
		{
			//reference-count is implicitly retained
		}
		virtual ~BaseObject() = default;

		virtual void* getBasePointer() = 0;

		const std::string typeName;
	protected:
		cl_uint referenceCount;

		friend class ObjectTracker;
	};

	template<typename BaseType, cl_int invalidObjectCode>
	class Object : public BaseObject
	{
	public:
		//make sure, objects can't be copied or moved, since it invalidates the pointers
		Object(const Object&) = delete;
		Object(Object&&) = delete;
		~Object() override = default;

		Object& operator=(const Object&) = delete;
		Object& operator=(Object&&) = delete;

		CHECK_RETURN cl_int retain()
		{
			if(!checkReferences())
				return returnError(invalidObjectCode, __FILE__, __LINE__, "Object reference check failed!");
			referenceCount += 1;
			return CL_SUCCESS;
		}

		CHECK_RETURN cl_int release()
		{
			if(!checkReferences())
				return returnError(invalidObjectCode, __FILE__, __LINE__, "Object reference check failed!");
			referenceCount -= 1;
			if(referenceCount == 0)
				ObjectTracker::removeObject(this);
			return CL_SUCCESS;
		}

		BaseType* toBase()
		{
			return &base;
		}

		const BaseType* toBase() const
		{
			return &base;
		}

		inline bool checkReferences() const
		{
			return referenceCount > 0;
		}

		inline cl_uint getReferences() const
		{
			return referenceCount;
		}

		void* getBasePointer() override
		{
			return reinterpret_cast<void*>(&base);
		}

	protected:
		BaseType base;

		Object() : BaseObject(BaseType::TYPE_NAME), base(this) { }
	};

	template<typename T>
	struct object_wrapper
	{
	public:
		constexpr object_wrapper() : ref(nullptr)
		{

		}

		explicit object_wrapper(T* object) : ref(object)
		{
			retainPointer();
		}

		object_wrapper(const object_wrapper& other) : ref(other.ref)
		{
			retainPointer();
		}

		object_wrapper(object_wrapper&& other) : ref(other.ref)
		{
			//neither retain (here) nor release (with destruction of the other wrapper)
			other.ref = nullptr;
		}

		~object_wrapper()
		{
			releasePointer();
		}

		object_wrapper& operator=(const object_wrapper& other)
		{
			if(ref == other.ref)
				return *this;

			releasePointer();
			ref = other.ref;
			retainPointer();

			return *this;
		}

		object_wrapper& operator=(object_wrapper&& other)
		{
			if(ref == other.ref)
				return *this;

			releasePointer();
			ref = other.ref;
			//neither retain (here) nor release (with destruction of the other wrapper)
			other.ref = nullptr;

			return *this;
		}

		T* get()
		{
			return ref;
		}

		const T* get() const
		{
			return ref;
		}

		T* operator->()
		{
			return ref;
		}

		const T* operator->() const
		{
			return ref;
		}

		explicit operator bool() const
		{
			return ref != nullptr;
		}

		void reset(T* ptr)
		{
			releasePointer();
			ref = ptr;
			retainPointer();
		}

	private:
		T* ref;

		void releasePointer()
		{
			if(ref != nullptr)
				ignoreReturnValue(ref->release(), __FILE__, __LINE__, "No way to handle error here!");
		}

		void retainPointer()
		{
			if(ref != nullptr && ref->retain() != CL_SUCCESS)
				throw std::runtime_error("Failed to retain object!");
		}
	};

	template<typename T, typename... Args>
	CHECK_RETURN inline T* newOpenCLObject(Args... args)
	{
		try
		{
			auto ptr = new T(args...);
			ObjectTracker::addObject(ptr);
			return ptr;
		}
		catch(std::bad_alloc& e)
		{
			//so we can return CL_OUT_OF_HOST_MEMORY
			return nullptr;
		}
	}
} // namespace vc4cl

#endif /* VC4CL_OBJECT_H */
