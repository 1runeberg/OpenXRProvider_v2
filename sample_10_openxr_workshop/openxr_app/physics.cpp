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

#include "physics.hpp"

namespace oxr
{
	namespace phy
	{
		Physics::Physics() 
		{
			// Register allocation hook
			JPH::RegisterDefaultAllocator();

			// Install callbacks
			JPH::Trace = TraceImpl;
			JPH_IF_ENABLE_ASSERTS( AssertFailed = AssertFailedImpl; )

			// Create a factory
			JPH::Factory::sInstance = new Factory();

			// Register all Jolt physics types
			JPH::RegisterTypes();
		}

		Physics::~Physics() 
		{
			// Unregisters all types with the factory and cleans up the default material
			UnregisterTypes();

			// Destroy the factory
			delete Factory::sInstance;
			Factory::sInstance = nullptr;

			if ( m_pTempAllocatorImpl )
				delete m_pTempAllocatorImpl;

			if ( m_pJobSystemThreadPool )
				delete m_pJobSystemThreadPool;

			if ( m_pPhysicsSystem )
				delete m_pPhysicsSystem;
		}


		void Physics::Init( Config &config ) 
		{
			// Register allocation hook
			JPH::RegisterDefaultAllocator();

			// Install callbacks
			JPH::Trace = TraceImpl;
			JPH_IF_ENABLE_ASSERTS( AssertFailed = AssertFailedImpl; )

			// Create a factory
			JPH::Factory::sInstance = new Factory();

			// Register all Jolt physics types
			JPH::RegisterTypes();

			// Temp allocator for temporary allocations during the physics update
			m_pTempAllocatorImpl = new JPH::TempAllocatorImpl( config.unTempAllocatorSize );

			// Job system that will execute physics jobs on multiple threads
			m_pJobSystemThreadPool = new JPH::JobSystemThreadPool ( 
				config.nMaxPhysicsJobs, 
				config.nMaxPhysicsBarriers, 
				config.nNumThreads );

			// Create the actual physics system
			m_pPhysicsSystem = new JPH::PhysicsSystem();
			m_pPhysicsSystem->Init( 
				config.unMaxBodies, 
				config.unNumBodyMutexes, 
				config.unMaxBodyPairs, 
				config.unMaxContactConstraints, 
				m_BPLInterfaceImpl, 
				m_ObjVsBplFilterImpl, 
				m_ObjPairFilterImpl );

			// Log info
			// todo: how to get jolt api version? prolly add to cmake?
			LogInfo( LOG_CATEGORY_PHYSICS, "Physics system initialized: JOLT" );
		}

		void Physics::Update( float fDeltaTime, int nCollissionSteps, int nIntegrationSubsteps ) 
		{ 
			if ( System() )
			{
				m_pPhysicsSystem->Update( fDeltaTime, nCollissionSteps, nIntegrationSubsteps, m_pTempAllocatorImpl, m_pJobSystemThreadPool );
			}
		}

		JPH::BodyInterface *Physics::BodyInterface() 
		{ 
			if ( !m_pPhysicsSystem )
				return nullptr; 

			return &m_pPhysicsSystem->GetBodyInterface(); 
		}

		bool Physics::CreateBox(
			PhysicsBody *outBody,
			JPH::RVec3Arg halfExtent,
			JPH::EActivation EActivation,
			JPH::RVec3Arg position,
			JPH::QuatArg rotation,
			JPH::EMotionType EMotionType,
			ObjectLayer objectLayer,
			bool bAddToWorld /* = true */,
			float fConvexRadius /*  = JPH::cDefaultConvexRadius */,
			const PhysicsMaterial *pMaterial /* = nullptr */ )
		{
			bool bCreated = CreatePhysicsBody( 
				outBody, 
				BodyCreationSettings( 
					new BoxShape( halfExtent, fConvexRadius, pMaterial ), 
					position, 
					rotation, 
					EMotionType, 
					objectLayer ), 
				EActivation, 
				bAddToWorld );

			if ( !bCreated )
				return false;

			return true;
		}

		bool Physics::CreateSphere(
			PhysicsBody *outBody,
			float fRadius,
			JPH::EActivation EActivation,
			JPH::RVec3Arg position,
			JPH::QuatArg rotation,
			EMotionType EMotionType,
			ObjectLayer objectLayer,
			bool bAddToWorld /* = true */,
			const PhysicsMaterial *pMaterial /* = nullptr */ )
		{
			bool bCreated = CreatePhysicsBody( 
					outBody, 
					BodyCreationSettings( 
						new SphereShape( fRadius, pMaterial ), 
						position, 
						rotation, 
						EMotionType, 
						objectLayer ), 
					EActivation, 
					bAddToWorld );

			if ( !bCreated )
				return false;

			return true;
		}
		
		bool Physics::CreatePhysicsBody( PhysicsBody *outPhysicsBody, JPH::BodyCreationSettings settings, JPH::EActivation EActivation, bool bAddToWorld /* = true */ ) 
		{ 
			// Reset state
			outPhysicsBody->bAddedToWorld = false;

			// Create rigid body
			outPhysicsBody->pBody = BodyInterface()->CreateBody( settings );
			if ( !outPhysicsBody->pBody )
			{
				LogError( LOG_CATEGORY_PHYSICS, "Error creating physics body. You may have ran out of physics body allocation." );
				return false;
			}

			// Set the body id		
			outPhysicsBody->Id = outPhysicsBody->pBody->GetID();

			if ( LogDebug )
				LogDebug( LOG_CATEGORY_PHYSICS, "New PhysicsBody created with ID: %i", ( uint32_t )outPhysicsBody->Id.GetIndex() );

			// Add rigid body to world if requested
			if ( bAddToWorld )
			{
				// Add it to the world
				BodyInterface()->AddBody( outPhysicsBody->Id, EActivation );
				outPhysicsBody->bAddedToWorld = true;

				if ( LogDebug )
					LogDebug( LOG_CATEGORY_PHYSICS, "PhysicsBody (%i) added to the world", ( uint32_t )outPhysicsBody->Id.GetIndex() );
			}

			return true; 
		}
	} // namespace phy

} // namespace oxr
