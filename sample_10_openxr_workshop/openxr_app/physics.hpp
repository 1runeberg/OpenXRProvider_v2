/* Copyright 2021, 2022, 2023 Rune Berg (GitHub: https://github.com/1runeberg, Twitter: https://twitter.com/1runeberg, YouTube: https://www.youtube.com/@1RuneBerg)
 *
 *  SPDX-License-Identifier: MIT
 *
 *  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *  DAMAGE.
 *
 */

#pragma once

#include <iostream>
#include <cstdarg>
#include <thread>

#include <provider/log.hpp>

// Jolt includes
#include <third_party/Jolt/Jolt.h>
#include <third_party/Jolt/RegisterTypes.h>
#include <third_party/Jolt/Core/Factory.h>
#include <third_party/Jolt/Core/TempAllocator.h>
#include <third_party/Jolt/Core/JobSystemThreadPool.h>
#include <third_party/Jolt/Physics/PhysicsSettings.h>
#include <third_party/Jolt/Physics/PhysicsSystem.h>
#include <third_party/Jolt/Physics/Collision/Shape/BoxShape.h>
#include <third_party/Jolt/Physics/Collision/Shape/SphereShape.h>
#include <third_party/Jolt/Physics/Body/BodyCreationSettings.h>
#include <third_party/Jolt/Physics/Body/BodyActivationListener.h>


// disable jolt common warnings
JPH_SUPPRESS_WARNINGS

// compile using single or double precision write 0.0_r to get a Real value that compiles to double or float depending if JPH_DOUBLE_PRECISION is set or not.
using namespace JPH::literals;
using namespace JPH;


namespace oxr
{
	class Renderable;

	namespace phy
	{
		#define LOG_CATEGORY_PHYSICS "Physics"

		static constexpr ObjectLayer OBJ_STATIC = 0;
		static constexpr ObjectLayer OBJ_DYNAMIC = 1;
		static constexpr ObjectLayer OBJ_COUNT = 2;

		static constexpr BroadPhaseLayer BPL_STATIC( 0 );
		static constexpr BroadPhaseLayer BPL_DYNAMIC( 1 );
		static constexpr uint BPL_COUNT( 2 );

		#ifdef JPH_ENABLE_ASSERTS
		// Callback for asserts
		static bool AssertFailedImpl( const char *inExpression, const char *inMessage, const char *inFile, JPH::uint inLine )
		{
			LogError( LOG_CATEGORY_PHYSICS, "Error in %s ln %s (%s)", inFile, inMessage, inExpression );
			return true;
		};
#endif // JPH_ENABLE_ASSERTS

		// Callback for traces
		static void TraceImpl( const char *inFMT, ... )
		{
			LogError( LOG_CATEGORY_PHYSICS, inFMT );
		}


		// BroadPhaseLayerInterface implementation
		// This defines a mapping between object and broadphase layers.
		class BPLInterfaceImpl final : public BroadPhaseLayerInterface
		{
		  public:
			BPLInterfaceImpl()
			{
				// Create a mapping table from object to broad phase layer
				mObjectToBroadPhase[ OBJ_STATIC ] = BPL_STATIC;
				mObjectToBroadPhase[ OBJ_DYNAMIC ] = BPL_DYNAMIC;
			}

			virtual uint GetNumBroadPhaseLayers() const override { return BPL_COUNT; }

			virtual BroadPhaseLayer GetBroadPhaseLayer( ObjectLayer inLayer ) const override
			{
				//JPH_ASSERT( inLayer < OBJ_COUNT );
				return mObjectToBroadPhase[ inLayer ];
			}

#if defined( JPH_EXTERNAL_PROFILE ) || defined( JPH_PROFILE_ENABLED )
			virtual const char *GetBroadPhaseLayerName( BroadPhaseLayer inLayer ) const override
			{
				switch ( ( BroadPhaseLayer::Type )inLayer )
				{
					case ( BroadPhaseLayer::Type ) BPL_STATIC:
						return "BPL_STATIC";
					case ( BroadPhaseLayer::Type )BPL_DYNAMIC:
						return "BPL_DYNAMIC";
					default:
						JPH_ASSERT( false );
						return "INVALID";
				}
			}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

		  private:
			BroadPhaseLayer mObjectToBroadPhase[ OBJ_COUNT ];
		};

		/// Class that determines if an object layer can collide with a broadphase layer
		class ObjVsBplFilterImpl : public ObjectVsBroadPhaseLayerFilter
		{
		  public:
			virtual bool ShouldCollide( ObjectLayer inLayer1, BroadPhaseLayer inLayer2 ) const override
			{
				switch ( inLayer1 )
				{
					case OBJ_STATIC:
						return inLayer2 == BPL_DYNAMIC;
					case OBJ_DYNAMIC:
						return true;
					default:
						JPH_ASSERT( false );
						return false;
				}
			}
		};

		// Class that determines if two object layers can collide
		class ObjPairFilterImpl : public ObjectLayerPairFilter
		{
		  public:

			bool bDebugMode = false;

			virtual bool ShouldCollide( ObjectLayer inObject1, ObjectLayer inObject2 ) const override
			{
				switch ( inObject1 )
				{
					case OBJ_STATIC:
						return inObject2 == OBJ_DYNAMIC; // Non moving only collides with moving
					case OBJ_DYNAMIC:
						return true; // Moving collides with everything
					default:
						JPH_ASSERT( false );
						return false;
				}
			}
		};

		// An example contact listener
		class ListenerContact : public ContactListener
		{
		  public:

			 bool bDebugMode = false;

			// See: ContactListener
			virtual ValidateResult OnContactValidate( const Body &inBody1, const Body &inBody2, RVec3Arg inBaseOffset, const CollideShapeResult &inCollisionResult ) override
			{
				if ( bDebugMode )
					LogDebug( LOG_CATEGORY_PHYSICS, "Contact PhysicsBody (%i) with PhysicsBody (%i) active on last update, but not in current.", ( uint32_t )inBody1.GetID().GetIndex(), ( uint32_t )inBody2.GetID().GetIndex() );

				// This allows an app to ignore a contact before it's created. Note that using layers is cheaper.
				return ValidateResult::AcceptAllContactsForThisBodyPair;
			}

			virtual void OnContactAdded( const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings ) override 
			{ 
				if ( bDebugMode )
					LogDebug( LOG_CATEGORY_PHYSICS, "Detected contact PhysicsBody (%i) with PhysicsBody (%i)", ( uint32_t )inBody1.GetID().GetIndex(), ( uint32_t )inBody2.GetID().GetIndex() );
			}

			virtual void OnContactPersisted( const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings ) override
			{
				if ( bDebugMode )
					LogDebug( LOG_CATEGORY_PHYSICS, "Persisted contact PhysicsBody (%i) with PhysicsBody (%i)", ( uint32_t )inBody1.GetID().GetIndex(), ( uint32_t )inBody2.GetID().GetIndex() );
			}

			virtual void OnContactRemoved( const SubShapeIDPair &inSubShapePair ) override 
			{ 		
				if ( bDebugMode )
					LogDebug(
						LOG_CATEGORY_PHYSICS,
						"Removed contact PhysicsBody (%i) with PhysicsBody (%i)",
						( uint32_t )inSubShapePair.GetBody1ID().GetIndex(),
						( uint32_t )inSubShapePair.GetBody2ID().GetIndex() );

			}
		};

		// An example activation listener
		class ListenerBodyActivation : public BodyActivationListener
		{
		  public:
			bool bDebugMode = false;

			virtual void OnBodyActivated( const BodyID &inBodyID, uint64 inBodyUserData ) override 
			{ 
				if ( bDebugMode )
					LogDebug( LOG_CATEGORY_PHYSICS, "PhysicsBody (%i) activated", ( uint32_t )inBodyID.GetIndex() );
			}

			virtual void OnBodyDeactivated( const BodyID &inBodyID, uint64 inBodyUserData ) override 
			{ 
				if ( bDebugMode )
					LogDebug( LOG_CATEGORY_PHYSICS, "PhysicsBody (%i) went to sleep", ( uint32_t )inBodyID.GetIndex() );
			}
		};

		struct Config
		{
			uint unTempAllocatorSize = 10 * 1024 * 1024;
			uint nMaxPhysicsJobs = JPH::cMaxPhysicsJobs;
			uint nMaxPhysicsBarriers = JPH::cMaxPhysicsBarriers;
			int nNumThreads = -1;

			uint unMaxBodies = 1024;
			uint unNumBodyMutexes = 0;
			uint unMaxBodyPairs = 1024;
			uint unMaxContactConstraints = 1024;
		};

		struct PhysicsBody
		{
			bool bAddedToWorld = false;
			JPH::Body *pBody = nullptr;
			JPH::BodyID Id;

			Renderable *pRenderable = nullptr;
		};

		class Physics
		{
		  public:
			Physics();
			~Physics();

			bool bDebugMode = false;
			
			void Init( Config &config );
			void Update( float fDeltaTime = 0.01667f, int nCollissionSteps = 1, int nIntegrationSubsteps = 1 );

			JPH::PhysicsSystem *System() { return m_pPhysicsSystem; }
			JPH::BodyInterface *BodyInterface();

			bool CreateBox( 
				PhysicsBody *outBody,
				JPH::RVec3Arg halfExtent, 
				JPH::EActivation EActivation, 
				JPH::RVec3Arg position,
				JPH::QuatArg rotation,
				EMotionType EMotionType,
				ObjectLayer objectLayer,
				bool bAddToWorld = true, 
				float fConvexRadius = JPH::cDefaultConvexRadius, 
				const PhysicsMaterial *pMaterial = nullptr );

			bool CreateSphere(
				PhysicsBody *outBody,
				float fRadius,
				JPH::EActivation EActivation,
				JPH::RVec3Arg position,
				JPH::QuatArg rotation,
				EMotionType EMotionType,
				ObjectLayer objectLayer,
				bool bAddToWorld = true,
				const PhysicsMaterial *pMaterial = nullptr );

		  private:
			BPLInterfaceImpl m_BPLInterfaceImpl;
			ObjVsBplFilterImpl m_ObjVsBplFilterImpl;
			ObjPairFilterImpl m_ObjPairFilterImpl;

			JPH::TempAllocatorImpl *m_pTempAllocatorImpl = nullptr;
			JPH::JobSystemThreadPool *m_pJobSystemThreadPool = nullptr;
			JPH::PhysicsSystem *m_pPhysicsSystem = nullptr;

			bool CreatePhysicsBody( PhysicsBody *outPhysicsBody, JPH::BodyCreationSettings settings, JPH::EActivation EActivation, bool bAddToWorld = true );	
		};

		
		static void SetDebugMode( bool bDebugMode, Physics &physicsSystem, ListenerContact &listenerContact, ListenerBodyActivation &listenetBodyActivation ) 
		{ 
			physicsSystem.bDebugMode = bDebugMode;
			listenerContact.bDebugMode = bDebugMode;
			listenetBodyActivation.bDebugMode = bDebugMode;
		}

	} // namespace phy

} // namespace oxr
