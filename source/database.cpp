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
#include "otpch.h"

#include <iostream>
#include <string>
#include <cstring>
#include "database.h"

#ifdef __USE_MYSQL__
#include "databasemysql.h"
#endif
#ifdef __USE_SQLITE__
#include "databasesqlite.h"
#endif

#if defined __USE_MYSQL__ && defined __USE_SQLITE__
#include "configmanager.h"
extern ConfigManager g_config;
#endif


OTSYS_THREAD_LOCKVAR DBQuery::database_lock;

DBQuery::DBQuery(){
	OTSYS_THREAD_LOCK(database_lock, NULL);
}

DBQuery::~DBQuery()
{
	OTSYS_THREAD_UNLOCK(database_lock, NULL);
}

DBResult::DBResult()
{
	m_numFields = 0;
	m_numRows = 0;
	/*m_lastNumFields = 0;*/
}

DBResult::~DBResult()
{
	clear();
}
void DBResult::addRow(char** values, unsigned long* lengths, unsigned int num_fields)
{
	RowData* rd = new RowData;
	rd->row = new char*[num_fields];
	rd->length = new unsigned long*[num_fields];

	for(unsigned int i=0; i < num_fields; ++i)
	{
		if(values[i] == NULL)
		{
			rd->row[i] = NULL;
			rd->length[i] = NULL;
			continue;
		}

		unsigned long colLen = lengths[i];
		rd->row[i] = new char[colLen + 1];
		rd->length[i] = new unsigned long(colLen);

		memcpy(rd->row[i], values[i], colLen);
		rd->row[i][colLen] = 0;
	}

	m_listRows[m_numRows] = rd;
	m_numRows++;
}
/*
void DBResult::clearRows()
{
	std::map<unsigned int, char **>::iterator it;
	for(it = m_listRows.begin(); it != m_listRows.end();)
	{
		for(unsigned int i=0; i < m_lastNumFields; ++i)
			delete[] it->second[i];

		delete[] it->second;
		m_listRows.erase(it++);
	}
	m_numRows = 0;
}

void DBResult::clearFieldNames()
{
	m_listNames.clear();
	m_lastNumFields = m_numFields;
	m_numFields = 0;
}
*/

void DBResult::clear()
{
	RowDataMap::iterator it;
	for(it = m_listRows.begin(); it != m_listRows.end();)
	{
		for(unsigned int i = 0; i < m_numFields; ++i){
			if(it->second->row[i] != NULL)
				delete[] it->second->row[i];

			if(it->second->length[i] != NULL)
				delete it->second->length[i];
		}

		delete[] it->second->row;
		delete[] it->second->length;
		delete it->second;
		m_listRows.erase(it++);
	}

	m_numRows = 0;
	m_listNames.clear();
	m_numFields = 0;
}

int DBResult::getDataInt(const std::string &s, unsigned int nrow)
{
	listNames_type::iterator it=m_listNames.find(s);
	if(it != m_listNames.end())
	{
		RowDataMap::iterator it2=m_listRows.find(nrow);
		if(it2 != m_listRows.end())
		{
			if(it2->second->row[it->second] == NULL)
				return 0;
			else
				return atoi(it2->second->row[it->second]);
		}
	}

	std::cout << "SQL ERROR DBResult::GetDataInt() " << s << std::endl;
	return 0; // Failed
}

long DBResult::getDataLong(const std::string &s, unsigned int nrow)
{
	listNames_type::iterator it=m_listNames.find(s);
	if(it != m_listNames.end())
	{
		RowDataMap::iterator it2=m_listRows.find(nrow);
		if(it2 != m_listRows.end())
		{
			if(it2->second->row[it->second] == NULL)
				return 0;
			else
				return atol(it2->second->row[it->second]);
		}
	}

	std::cout << "SQL ERROR DBResult::GetDataLong() " << s << std::endl;
	return 0; // Failed
}


std::string DBResult::getDataString(const std::string &s, unsigned int nrow)
{
	listNames_type::iterator it=m_listNames.find(s);
	if(it != m_listNames.end())
	{
		RowDataMap::iterator it2=m_listRows.find(nrow);
		if(it2 != m_listRows.end())
		{
			if(it2->second->row[it->second] == NULL)
				return std::string("");
			else
				return std::string(it2->second->row[it->second]);
		}
	}

	std::cout << "SQL ERROR DBResult::GetDataString() " << s << std::endl;
	return std::string(""); // Failed
}

const char* DBResult::getDataBlob(const std::string &s, unsigned long& size, unsigned int nrow)
{
	listNames_type::iterator it=m_listNames.find(s);
	if(it != m_listNames.end())
	{
		RowDataMap::iterator it2=m_listRows.find(nrow);
		if(it2 != m_listRows.end())
		{
			if(it2->second->row[it->second] == NULL){
				size = 0;
				return NULL;
			}
			else{
				size = *it2->second->length[it->second];
				return it2->second->row[it->second];
			}
		}
	}

	std::cout << "SQL ERROR DBResult::getDataBlob() " << s << std::endl;
	size = 0;
	return NULL;
}

Database* _Database::_instance = NULL;

Database* _Database::instance(){
	if(!_instance){
#if defined __USE_MYSQL__ && defined __USE_SQLITE__
        if(g_config.getString(ConfigManager::SQL_TYPE) == "mysql"){
            _instance = new DatabaseMySQL;
		}
        else{
            _instance = new DatabaseSqLite;
		}
#elif defined __USE_MYSQL__
		_instance = new DatabaseMySQL;
#elif defined __USE_SQLITE__
		_instance = new DatabaseSqLite;
#endif
		OTSYS_THREAD_LOCKVARINIT(DBQuery::database_lock);
	}
	return _instance;
}

#ifdef __USE_SQLITE__
// SQLite has no backslash escapes: a quote is doubled, NUL cannot be part of the query text.
static std::string sqliteEscapeString(const char* s, unsigned long size)
{
	std::string r;
	r.reserve(size);
	for(unsigned long i = 0; i < size; ++i){
		if(s[i] == '\'')
			r += "''";
		else if(s[i] != '\0')
			r += s[i];
	}
	return r;
}
#endif

#ifdef __USE_MYSQL__
static std::string mysqlEscapeString(const char* s, unsigned long size)
{
	char* output = new char[size * 2 + 1];

	mysql_escape_string(output, s, size);
	std::string r = std::string(output);
	delete[] output;
	return r;
}
#endif

std::string _Database::escapeString(const std::string &s)
{
	return escapeString(s.c_str(), s.size());
}

std::string _Database::escapeString(const char* s, unsigned long size)
{
	if(s == NULL)
		return std::string("");

#if defined __USE_MYSQL__ && defined __USE_SQLITE__
	if(instance()->isSQLite())
		return sqliteEscapeString(s, size);
	return mysqlEscapeString(s, size);
#elif defined __USE_MYSQL__
	return mysqlEscapeString(s, size);
#else
	return sqliteEscapeString(s, size);
#endif
}

std::string _Database::escapeBlob(const char* s, unsigned long size)
{
#ifdef __USE_SQLITE__
	if(s != NULL && size > 0 && instance()->isSQLite()){
		static const char hex[] = "0123456789ABCDEF";
		std::string r = "X'";
		r.reserve(size * 2 + 3);
		for(unsigned long i = 0; i < size; ++i){
			r += hex[(unsigned char)s[i] >> 4];
			r += hex[(unsigned char)s[i] & 0x0F];
		}
		return r + "'";
	}
#endif
	return "'" + escapeString(s, size) + "'";
}

DBTransaction::DBTransaction(Database* database)
{
	m_database = database;
	m_state = STATE_NO_START;
}

DBTransaction::~DBTransaction()
{
	if(m_state == STATE_START){
		if(!m_database->rollback()){
			//TODO: What to do here?
		}
	}
}

bool DBTransaction::start()
{
	DBQuery query;
	if(m_database->isSQLite())
		query << "BEGIN;";
	else
		query << "START TRANSACTION;";
	if(m_database->executeQuery(query)){
		m_state = STATE_START;
		return true;
	}
	else{
		return false;
	}
}

bool DBTransaction::success()
{
	if(m_state == STATE_START){
		m_state = STEATE_COMMIT;
		return m_database->commit();
	}
	else{
		return false;
	}
}


DBSplitInsert::DBSplitInsert(Database* database)
{
	m_database = database;
}

DBSplitInsert::~DBSplitInsert()
{
	//
}

void DBSplitInsert::clear()
{
	m_buffer.clear();
	m_buffer.reserve(10240);
}

void DBSplitInsert::setQuery(const std::string& query)
{
	clear();
	m_query = query;
}

bool DBSplitInsert::addRow(const std::string& row)
{
#ifdef __SPLIT_QUERIES__

	m_buffer = row;
	bool ret = internalExecuteQuery();
	m_buffer = "";
	return ret;

#else
	int size = m_buffer.size();
	if(size == 0){
		m_buffer = row;
	}
	else if(size > 8192){
		if(!internalExecuteQuery()){
			return false;
		}
		m_buffer = row;
	}
	else{
		m_buffer += "," + row;
	}
	return true;

#endif
}


bool DBSplitInsert::internalExecuteQuery()
{
	DBQuery subquery;
	subquery << m_query;
	subquery << m_buffer;

	if(!m_database->executeQuery(subquery)){
		return false;
	}
	else{
		return true;
	}
}

bool DBSplitInsert::executeQuery()
{
	if(m_buffer.size() != 0){
		return internalExecuteQuery();
	}
	return true;
}
