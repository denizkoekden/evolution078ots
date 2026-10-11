//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
//
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

#include <iostream>
#include <vector>
#include "databasesqlite.h"
#include "configmanager.h"

extern ConfigManager g_config;

DatabaseSqLite::DatabaseSqLite()
{
	init();
}

DatabaseSqLite::~DatabaseSqLite()
{
	disconnect();
}

bool DatabaseSqLite::init()
{
	m_initialized = false;
	m_connected = false;

	// Initialize mysql
	if(sqlite3_open(g_config.getString(ConfigManager::SQLITE_DB).c_str(), &m_handle) != SQLITE_OK){
		//throw DBError("sqlite_init", DB_ERROR_INIT);
		std::cout << "SQLITE ERROR sqlite_init" << std::endl;
		sqlite3_close(m_handle);
	}
	else
		m_initialized = true;

	return m_initialized;
}

bool DatabaseSqLite::connect()
{
    //don't need to connect
	m_connected = true;
	return true;
}

bool DatabaseSqLite::disconnect()
{
	if(m_initialized){
		sqlite3_close(m_handle);
		m_initialized = false;
		return true;
	}

	return false;
}

// One statement per call: unlike sqlite3_exec, trailing statements are rejected, not run.
bool DatabaseSqLite::prepare(const std::string& query, sqlite3_stmt*& stmt)
{
	const char* tail = NULL;
	stmt = NULL;
	if(sqlite3_prepare_v2(m_handle, query.c_str(), (int)query.length(), &stmt, &tail) != SQLITE_OK){
		std::cout << "SQLITE ERROR sqlite3_prepare: " << query << " " << sqlite3_errmsg(m_handle) << std::endl;
		return false;
	}

	for(; tail && *tail; ++tail){
		if(*tail != ' ' && *tail != '\t' && *tail != '\r' && *tail != '\n' && *tail != ';'){
			std::cout << "SQLITE ERROR multiple statements: " << query << std::endl;
			sqlite3_finalize(stmt);
			stmt = NULL;
			return false;
		}
	}
	return true;
}

bool DatabaseSqLite::executeQuery(DBQuery &q)
{
	if(!m_initialized || !m_connected)
		return false;

	std::string s = q.str();
	sqlite3_stmt* stmt;
	if(!prepare(s, stmt))
		return false;

	int rc = SQLITE_DONE;
	if(stmt){
		while((rc = sqlite3_step(stmt)) == SQLITE_ROW){}
		if(rc != SQLITE_DONE)
			std::cout << "SQLITE ERROR sqlite3_step: " << s << " " << sqlite3_errmsg(m_handle) << std::endl;
		sqlite3_finalize(stmt);
	}

	if(rc != SQLITE_DONE)
		return false;

	// All is ok
	q.reset();
	return true;
}

bool DatabaseSqLite::storeQuery(DBQuery &q, DBResult &dbres)
{
	if(!m_initialized || !m_connected)
		return false;

	std::string s = q.str();

	q.reset();
	dbres.clear();

	sqlite3_stmt* stmt;
	if(!prepare(s, stmt) || !stmt)
		return false;

	unsigned int num_fields = (unsigned int)sqlite3_column_count(stmt);
	for(unsigned int i = 0; i < num_fields; ++i)
		dbres.setFieldName(std::string(sqlite3_column_name(stmt, i)), i);

	std::vector<char*> values(num_fields + 1);
	std::vector<unsigned long> lengths(num_fields + 1);
	int rc;
	while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
		for(unsigned int i = 0; i < num_fields; ++i){
			if(sqlite3_column_type(stmt, i) == SQLITE_NULL){
				values[i] = NULL;
				lengths[i] = 0;
			}
			else{
				// text before bytes: the length is that of the returned buffer, blobs keep embedded NULs
				const unsigned char* text = sqlite3_column_text(stmt, i);
				lengths[i] = (unsigned long)sqlite3_column_bytes(stmt, i);
				values[i] = text ? (char*)text : (char*)"";
			}
		}
		dbres.addRow(&values[0], &lengths[0], num_fields);
	}

	if(rc != SQLITE_DONE){
		std::cout << "SQLITE ERROR sqlite3_step: " << s << " " << sqlite3_errmsg(m_handle) << std::endl;
		sqlite3_finalize(stmt);
		return false;
	}
	sqlite3_finalize(stmt);

	// Check if there are rows in the query
	if(dbres.getNumRows() > 0)
		return true;
	else
		return false;
}

bool DatabaseSqLite::rollback()
{
    DBQuery query;
	query << "ROLLBACK;";

	if(executeQuery(query)){
		return true;
	}
	else{
		return false;
	}
}

bool DatabaseSqLite::commit()
{
    DBQuery query;
	query << "COMMIT;";

	if(executeQuery(query)){
		return true;
	}
	else{
		return false;
	}
}
