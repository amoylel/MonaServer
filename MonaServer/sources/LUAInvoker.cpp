/*
Copyright 2014 Mona
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

This file is a part of Mona.
*/

#include "LUAInvoker.h"
#include "LUAPublication.h"
#include "LUAUDPSocket.h"
#include "LUATCPClient.h"
#include "LUATCPServer.h"
#include "LUAGroup.h"
#include "LUAMember.h"
#include "LUAFilePath.h"
#include "LUABroadcaster.h"
#include "Mona/Exceptions.h"
#include "Mona/Files.h"
#include "MonaServer.h"
#include <openssl/evp.h>
#include "Mona/JSONReader.h"
#include "Mona/JSONWriter.h"
#include "Mona/XMLReader.h"
#include "Mona/XMLWriter.h"
#include "math.h"


using namespace std;
using namespace Mona;

// HERE JUST TO SET THE COLLECTOR FOR EVERY COLLECTIONS
void LUAInvoker::Init(lua_State *pState, Invoker& invoker) {
	Script::Collection<Invoker,LUAClient>(pState, -1, "clients",&invoker);
	lua_setfield(pState, -2,"clients");

	Script::Collection<Invoker,LUAGroup>(pState, -1, "groups",&invoker);
	lua_setfield(pState, -2,"groups");
	
	Parameters::ForEach pushProperty([pState](const string& key, const string& value) {
		Script::PushKeyValue(pState,key, value);
	});

	// Configs
	Script::Collection(pState,-1, "configs");
	Script::FillCollection(pState, invoker.iterate(pushProperty));
	lua_setfield(pState, -2,"configs");

	// Environement
	Script::Collection(pState,-1, "environment");
	Script::FillCollection(pState, Util::Environment().iterate(pushProperty));
	lua_setfield(pState, -2,"environment");
}


void LUAInvoker::AddClient(lua_State *pState) {
	// -1 must be the client table!
	lua_getglobal(pState, "mona");
	Script::Collection(pState, -1, "clients");
	lua_getfield(pState, -3,"id");
	lua_pushvalue(pState, -4); // client table
	Script::FillCollection(pState,1);
	lua_pop(pState, 2);
}

void LUAInvoker::RemoveClient(lua_State *pState) {
	// -1 must be the client table!
	lua_getglobal(pState, "mona");
	Script::Collection(pState, -1, "clients");
	lua_getmetatable(pState, -3);
	lua_getfield(pState, -1, "|id");
	lua_replace(pState, -2);
	lua_pushnil(pState);
	Script::FillCollection(pState,1);
	lua_pop(pState, 2);
}

void LUAInvoker::AddPublication(lua_State *pState, const Publication& publication) {
	// -1 must be the publication table!
	lua_getglobal(pState, "mona");
	Script::Collection(pState, -1, "publications");
	lua_pushstring(pState,publication.name().c_str());
	lua_pushvalue(pState, -4);
	Script::FillCollection(pState,1);
	lua_pop(pState, 2);
}

void LUAInvoker::RemovePublication(lua_State *pState, const Publication& publication) {
	lua_getglobal(pState, "mona");
	Script::Collection(pState, -1, "publications");
	lua_pushstring(pState,publication.name().c_str());
	lua_pushnil(pState);
	Script::FillCollection(pState,1);
	lua_pop(pState, 2);
}

void LUAInvoker::AddGroup(lua_State *pState) {
	// -1 must be the group table!
	lua_getglobal(pState, "mona");
	Script::Collection(pState, -1, "groups");
	lua_getfield(pState, -3, "id");
	lua_pushvalue(pState, -4);
	Script::FillCollection(pState,1);
	lua_pop(pState, 2);
}

void LUAInvoker::RemoveGroup(lua_State *pState) {
	// -1 must be the group table!
	lua_getglobal(pState, "mona");
	Script::Collection(pState, -1, "groups");
	lua_getmetatable(pState, -3);
	lua_getfield(pState, -1, "|id");
	lua_replace(pState, -2);
	lua_pushnil(pState);
	Script::FillCollection(pState,1);
	lua_pop(pState, 2);
}


int	LUAInvoker::Split(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		string expression = SCRIPT_READ_STRING("");
		string separator = SCRIPT_READ_STRING("");
		String::ForEach forEach([__pState](const char* value) {
			SCRIPT_WRITE_STRING(value)
		});
		String::Split(expression, separator, forEach,SCRIPT_READ_UINT(0));
	SCRIPT_CALLBACK_RETURN
}

int	LUAInvoker::Dump(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker, invoker)
		SCRIPT_READ_BINARY(data, size)
		UInt32 count(SCRIPT_READ_UINT(size));
		Logs::Dump(data,count>size ? size : count);
	SCRIPT_CALLBACK_RETURN
}


int	LUAInvoker::Publish(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
	string name = SCRIPT_READ_STRING("");
	Exception ex;
	
	Publication* pPublication = invoker.publish(ex, name,strcmp(SCRIPT_READ_STRING("live"),"record")==0 ? Publication::RECORD : Publication::LIVE);

	if (!pPublication)
		SCRIPT_ERROR(ex ? ex.error().c_str() : "Unknown error")
	else {
		if (ex)
			SCRIPT_WARN(ex.error().c_str())
		SCRIPT_NEW_OBJECT(Publication, LUAPublication, pPublication)
		lua_getmetatable(pState, -1);
		lua_pushlightuserdata(pState, &invoker);
		lua_setfield(pState, -2,"|invoker");
		lua_pop(pState, 1);
	}
	SCRIPT_CALLBACK_RETURN
}

int	LUAInvoker::AbsolutePath(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		SCRIPT_WRITE_STRING((MonaServer::WWWPath + "/" + SCRIPT_READ_STRING("") + "/").c_str())
	SCRIPT_CALLBACK_RETURN
}

int	LUAInvoker::CreateUDPSocket(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		SCRIPT_NEW_OBJECT(LUAUDPSocket,LUAUDPSocket,new LUAUDPSocket(invoker.sockets,SCRIPT_READ_BOOL(false),pState))
	SCRIPT_CALLBACK_RETURN
}

int	LUAInvoker::CreateTCPClient(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		SCRIPT_NEW_OBJECT(LUATCPClient, LUATCPClient, new LUATCPClient(invoker.sockets, pState))
	SCRIPT_CALLBACK_RETURN
}

int	LUAInvoker::CreateTCPServer(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		SCRIPT_NEW_OBJECT(LUATCPServer, LUATCPServer, new LUATCPServer(invoker.sockets, pState))
	SCRIPT_CALLBACK_RETURN
}

int	LUAInvoker::Md5(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		while(SCRIPT_CAN_READ) {
			SCRIPT_READ_BINARY(data,size)
			if(data) {
				UInt8 result[16];
				EVP_Digest(data,size,result,NULL,EVP_md5(),NULL);
				SCRIPT_WRITE_BINARY(result, 16);
			} else {
				SCRIPT_ERROR("Input MD5 value have to be a string expression")
				SCRIPT_WRITE_NIL
			}
		}
	SCRIPT_CALLBACK_RETURN
}

int LUAInvoker::ListFiles(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		string path(MonaServer::WWWPath + "/" + SCRIPT_READ_STRING("") + "/");
		Exception ex;
		Files dir(ex, path);
		UInt32 index = 0;
		SCRIPT_NEW_OBJECT(LUAFiles, LUAFiles, new LUAFiles())
		for(auto itFile = dir.begin(); itFile != dir.end(); ++itFile) {
			SCRIPT_NEW_OBJECT(LUAFilePath, LUAFilePath, new LUAFilePath(*itFile))
			lua_rawseti(pState,-2,++index);
		}
	SCRIPT_CALLBACK_RETURN
}

int	LUAInvoker::Sha256(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		while(SCRIPT_CAN_READ) {
			SCRIPT_READ_BINARY(data,size)
			if(data) {
				UInt8 result[32];
				EVP_Digest(data,size,result,NULL,EVP_sha256(),NULL);
				SCRIPT_WRITE_BINARY(result,32);
			} else {
				SCRIPT_ERROR("Input SHA256 value have to be a string expression")
				SCRIPT_WRITE_NIL
			}
		}
	SCRIPT_CALLBACK_RETURN
}

int	LUAInvoker::ToAMF0(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		AMFWriter writer(invoker.poolBuffers);
		writer.amf0=true;
		SCRIPT_READ_DATA(writer)
		SCRIPT_WRITE_BINARY(writer.packet.data(),writer.packet.size())
	SCRIPT_CALLBACK_RETURN
}

int	LUAInvoker::AddToBlacklist(lua_State* pState) {
	SCRIPT_CALLBACK(Invoker,invoker)	
		while(SCRIPT_CAN_READ) {
			IPAddress address;
			Exception ex;
			bool success;
			EXCEPTION_TO_LOG(success=address.set(ex, SCRIPT_READ_STRING("")), "Blacklist entry")
			if (success)
				invoker.addBanned(address);
		}
	SCRIPT_CALLBACK_RETURN
}

int	LUAInvoker::RemoveFromBlacklist(lua_State* pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		while(SCRIPT_CAN_READ) {
			IPAddress address;
			Exception ex;
			bool success;
			EXCEPTION_TO_LOG(success = address.set(ex, SCRIPT_READ_STRING("")), "Blacklist entry")
			if (success)
				invoker.removeBanned(address);
		}
	SCRIPT_CALLBACK_RETURN
}

int LUAInvoker::JoinGroup(lua_State* pState) {
	SCRIPT_CALLBACK(Invoker, invoker)
		SCRIPT_READ_BINARY(peerId, size)
		Buffer buffer(ID_SIZE);
		if (size == (ID_SIZE << 1)) {
			memcpy(buffer.data(),peerId,size);
			peerId = Util::UnformatHex(buffer).data();
		} else if (size != ID_SIZE) {
			if (peerId) {
				SCRIPT_ERROR("Bad member format id ", string((const char*)peerId, size));
				peerId = NULL;
			} else
				SCRIPT_ERROR("Member id argument missing");
		}
		if (peerId) {
			SCRIPT_READ_BINARY(groupId, size)
			if (size == (ID_SIZE << 1)) {
				memcpy(buffer.data(),groupId,size);
				groupId = Util::UnformatHex(buffer).data();
			} else if (size != ID_SIZE) {
				if (groupId) {
					SCRIPT_ERROR("Bad group format id ", string((const char*)groupId, size))
					groupId = NULL;
				} else
					SCRIPT_ERROR("Group id argument missing")
			}
			if (groupId) {
				Peer* pPeer = new Peer((Handler&)invoker);
				memcpy((void*)pPeer->id, peerId, ID_SIZE);
				pPeer->joinGroup(groupId, NULL);
				SCRIPT_NEW_OBJECT(Peer, LUAMember, pPeer)
			}
		}
	SCRIPT_CALLBACK_RETURN
}

int LUAInvoker::Get(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		const char* name = SCRIPT_READ_STRING(NULL);
		if (name) {
			if(strcmp(name,"clients")==0) {
				Script::Collection(pState,1,"clients");
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "dump") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::Dump)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "joinGroup") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::JoinGroup)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "groups") == 0) {
				Script::Collection(pState, 1, "groups");
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "publications") == 0) {
				Script::Collection(pState, 1, "publications");
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "publish") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::Publish)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "toAMF") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::ToData<Mona::AMFWriter>)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "toAMF0") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::ToAMF0)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "fromAMF") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::FromData<Mona::AMFReader>)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "toJSON") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::ToData<Mona::JSONWriter>)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "fromJSON") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::FromData<Mona::JSONReader>)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "toXML") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::ToData<Mona::XMLWriter>)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "fromXML") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::FromData<XMLReader>)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "absolutePath") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::AbsolutePath)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "epochTime") == 0) {
				SCRIPT_WRITE_NUMBER(Time::Now())
			} else if (strcmp(name, "split") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::Split)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "createUDPSocket") == 0) {
 				SCRIPT_WRITE_FUNCTION(LUAInvoker::CreateUDPSocket)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name, "createTCPClient") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::CreateTCPClient)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if(strcmp(name,"createTCPServer")==0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::CreateTCPServer)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if(strcmp(name,"md5")==0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::Md5)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if(strcmp(name,"sha256")==0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::Sha256)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if(strcmp(name,"addToBlacklist")==0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::AddToBlacklist)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if(strcmp(name,"removeFromBlacklist")==0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::RemoveFromBlacklist)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name,"configs")==0) {
				Script::Collection(pState,1, "configs");
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name,"environment")==0) {
				Script::Collection(pState,1, "environment");
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if(strcmp(name,"servers")==0) {
				lua_getmetatable(pState, 1);
				lua_getfield(pState, -1, "|servers");
				lua_replace(pState, -2);
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else if (strcmp(name,"dir")==0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::ListFiles)
				SCRIPT_CALLBACK_FIX_INDEX(name)
			} else {
				Script::Collection(pState,1, "configs");
				lua_getfield(pState, -1, name);
				lua_replace(pState, -2);
				SCRIPT_CALLBACK_FIX_INDEX(name)
			}
		}
	SCRIPT_CALLBACK_RETURN
}

int LUAInvoker::Set(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		lua_rawset(pState,1); // consumes key and value
	SCRIPT_CALLBACK_RETURN
}

