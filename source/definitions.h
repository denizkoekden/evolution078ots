//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// various definitions needed by most files
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////


#ifndef __OTSERV_DEFINITIONS_H__
#define __OTSERV_DEFINITIONS_H__

#include "exception.h"
#include "configmanager.h"

extern ConfigManager g_config;

#ifdef XML_GCC_FREE
	#define xmlFreeOTSERV(s)	free(s)
#else
	#define xmlFreeOTSERV(s)	xmlFree(s)
#endif

#ifdef __DEBUG_EXCEPTION_REPORT__
	#define DEBUG_REPORT int *a = NULL; *a = 1;
#else
	#ifdef __EXCEPTION_TRACER__
		#define DEBUG_REPORT ExceptionHandler::dumpStack();
	#else
		#define DEBUG_REPORT
	#endif
#endif

#ifdef __USE_SQLITE__
    #define __SPLIT_QUERIES__
#endif

#if defined __USE_MYSQL__ || __USE_SQLITE__
	#define USE_SQL_ENGINE
#endif

#if defined(__USE_MYSQL__) && !defined(__USE_SQLITE__)
	#define USE_MYSQL_ONLY
#endif

#ifdef __XID_PREMIUM_SYSTEM__
	#define FREE_PREMIUM g_config.getString(ConfigManager::FREE_PREMIUM)
#endif

#ifdef __XID_PROTECTION_SYSTEM__
	#define PROTECTION_LIMIT g_config.getNumber(ConfigManager::PROTECTION_LIMIT)
#endif

#ifdef __XID_CONFIG_CAP__
	#define CAP_SYSTEM g_config.getString(ConfigManager::CAP_SYSTEM)
#else
	#define CAP_SYSTEM "yes"
#endif

#ifdef __PARTYSYSTEM__
	#ifndef __SKULLSYSTEM__
		#undef __PARTYSYSTEM__
	#endif
#endif

#define ACCESS_ENTER g_config.getNumber(ConfigManager::ACCESS_ENTER)
#define ACCESS_PROTECT g_config.getNumber(ConfigManager::ACCESS_PROTECT)
#define ACCESS_HOUSE g_config.getNumber(ConfigManager::ACCESS_HOUSE)
#define ACCESS_TALK g_config.getNumber(ConfigManager::ACCESS_TALK)
#define ACCESS_MOVE g_config.getNumber(ConfigManager::ACCESS_MOVE)
#define ACCESS_LOOK g_config.getNumber(ConfigManager::ACCESS_LOOK)

#define GUILD_SYSTEM g_config.getString(ConfigManager::GUILD_SYSTEM)

#define CRITICAL_CHANCE g_config.getCriticalString(1)
#define CRITICAL_DAMAGE g_config.getCriticalString(2)

#define ipText(a) (unsigned int)a[0] << "." << (unsigned int)a[1] << "." << (unsigned int)a[2] << "." << (unsigned int)a[3]

// Fixed-width integer types — portable replacement for the original hand-rolled
// typedefs (typedef __int64 int64_t; typedef unsigned long uint32_t; ...) that
// only worked on the 2007 MinGW/MSVC toolchains. <cstdint> yields the exact same
// widths on every modern toolchain.
#include <cstdint>

// assert() was reached transitively through the old toolchain's headers in many
// TUs; modern libstdc++/libc++ no longer leak it. definitions.h is included almost
// everywhere, so pull it in once here (behaviour-neutral).
#include <cassert>

// Hash containers — the original used GCC's __gnu_cxx::hash_map / hash_set (via
// <ext/hash_map>) and MSVC's stdext::hash_map, both removed or hard-deprecated on
// modern compilers. std::unordered_map/set has the same average-O(1) find/insert/
// iterate semantics; no call site here depends on iteration order, so this is a
// 1:1 behavioural swap.
#include <unordered_map>
#include <unordered_set>
#define OTSERV_HASH_MAP std::unordered_map
#define OTSERV_HASH_SET std::unordered_set

#if defined __WINDOWS__ || defined WIN32

	#ifndef __FUNCTION__
		#define	__FUNCTION__ __func__
	#endif

	#define OTSYS_THREAD_RETURN  void
	#define EWOULDBLOCK WSAEWOULDBLOCK

	#ifdef _MSC_VER
		#include <cstring>
		inline int strcasecmp(const char *s1, const char *s2)
		{
			return ::_stricmp(s1, s2);
		}

		#pragma warning(disable:4786) // msvc too long debug names in stl
		#pragma warning(disable:4250) // 'class1' : inherits 'class2::member' via dominance
	#endif

//*nix systems
#else
	#define OTSYS_THREAD_RETURN void*

	#include <string.h>
#endif

#endif
