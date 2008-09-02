//
// This file is used to handle alerts generated by GreenSQL.
//
// Copyright (c) 2007 GreenSQL.NET <stremovsky@gmail.com>
// License: GPL v2 (http://www.gnu.org/licenses/gpl.html)
//

#include "config.hpp"
#include "log.hpp"

static unsigned int agroup_get(int p_id, std::string & dbn, std::string & p);
static unsigned int agroup_add(int p_id, std::string & dbn, std::string & p);
static int alert_add(unsigned int agroupid, char * user, char * query, 
                     char * reason, int risk, int block);
static bool agroup_update(unsigned int agroupid);
static bool agroup_update_status(unsigned int agroupid);

    
bool logalert(int proxy_id, std::string & dbname,  std::string & dbuser,
        std::string & query, std::string & pattern, 
        std::string & reason, int risk, int block)
{
    GreenSQLConfig * conf = GreenSQLConfig::getInstance();
    MYSQL * dbConn = &conf->dbConn;

    // when mysql_real_escape_string function escapes binary zero it is changed to
    // \x00 . As a result, original string after escaping can grow up to 4 times.

    char * tmp_q = new char[query.length()*4+1];
    mysql_real_escape_string(dbConn, tmp_q, query.c_str(), (unsigned long) query.length());
    tmp_q[query.length()*4] = '\0';
    
    char * tmp_u = new char[dbuser.length()*4+1];
    mysql_real_escape_string(dbConn, tmp_u, dbuser.c_str(), (unsigned long) dbuser.length());
    tmp_u[dbuser.length()*4] ='\0';
        
    char * tmp_r = new char[reason.length()*4+1];
    mysql_real_escape_string(dbConn, tmp_r, reason.c_str(), (unsigned long) reason.length()); 
    tmp_r[reason.length()*4] = '\0';

    unsigned int agroupid = agroup_get(proxy_id, dbname, pattern);
    if (agroupid == 0)
    {
        agroupid = agroup_add(proxy_id, dbname, pattern);
    }
    //failed to add
    if (agroupid == 0)
    {
        logevent(ERR, "Failed to get group alert id\n");
        delete [] tmp_q;
        delete [] tmp_u;
        delete [] tmp_r;
        return false;
    }
    alert_add(agroupid, tmp_u, tmp_q, tmp_r, risk, block);
    agroup_update(agroupid);
    delete [] tmp_q;
    delete [] tmp_u;
    delete [] tmp_r;
    return true;    
}

bool logwhitelist(int proxy_id, std::string & dbname,  std::string & dbuser,
        std::string & query, std::string & pattern,
        std::string & reason, int risk, int block)
{
    GreenSQLConfig * conf = GreenSQLConfig::getInstance();
    MYSQL * dbConn = &conf->dbConn;

    // when mysql_real_escape_string function escapes binary zero it is changed to
    // \x00 . As a result, original string after escaping can grow up to 4 times.

    char * tmp_q = new char[query.length()*4+1];
    mysql_real_escape_string(dbConn, tmp_q, query.c_str(), (unsigned long) query.length());
    tmp_q[query.length()*4] = '\0';

    char * tmp_u = new char[dbuser.length()*4+1];
    mysql_real_escape_string(dbConn, tmp_u, dbuser.c_str(), (unsigned long) dbuser.length());
    tmp_u[dbuser.length()*4] ='\0';

    char * tmp_r = new char[reason.length()*4+1];
    mysql_real_escape_string(dbConn, tmp_r, reason.c_str(), (unsigned long) reason.length());
    tmp_r[reason.length()*4] = '\0';


    unsigned int agroupid = agroup_get(proxy_id, dbname, pattern);
    if (agroupid == 0)
    {
        agroupid = agroup_add(proxy_id, dbname, pattern);
    }
    //failed to add
    if (agroupid == 0)
    {
        logevent(ERR, "Failed to get group alert id\n");
        delete [] tmp_q;
        delete [] tmp_u;
        delete [] tmp_r;
        return false;
    }
    alert_add(agroupid, tmp_u, tmp_q, tmp_r, risk, block);
    agroup_update_status(agroupid);
    delete [] tmp_q;
    delete [] tmp_u;
    delete [] tmp_r;
    return true;
}

static unsigned int 
agroup_get(int proxy_id, std::string & dbname,std::string & pattern)
{
//    char q[pattern.length() + 1024];
    char * q = new char[pattern.length() + 1024];

    GreenSQLConfig * conf = GreenSQLConfig::getInstance();
    MYSQL * dbConn = &conf->dbConn;
    MYSQL_RES *res; /* To be used to fetch information into */
    MYSQL_ROW row;

    //first check we we have similar alert in the past
    snprintf(q, pattern.length() + 1024, 
        "SELECT agroupid FROM alert_group WHERE "
        "proxyid = %d and db_name = '%s' and pattern = '%s'",
        proxy_id, dbname.c_str(), pattern.c_str());

    /* read new urls from the database */
    if( mysql_query(dbConn, q) )
    {
        /* Make query */
        logevent(STORAGE,"(%s) %s\n", q, mysql_error(dbConn));
        delete [] q;
        return 0;
    }
    delete [] q;
    q = NULL;

    /* Download result from server */
    res=mysql_store_result(dbConn);
    if (res == NULL)
    {
        //logevent(STORAGE, "Records Found: 0, error:%s\n", 
        //           mysql_error(dbConn));
        return 0;
    }
    //my_ulonglong mysql_num_rows(MYSQL_RES *result)
    //logevent(STORAGE, "Records Found: %lld\n", mysql_num_rows(res) );
    if (mysql_num_rows(res) > 0)
    {
        row=mysql_fetch_row(res);
        unsigned int i = atoi(row[0]);
    
        /* Release memory used to store results. */
        mysql_free_result(res);
        return i;
    }
    /* Release memory used to store results. */
    mysql_free_result(res);
    return 0;
}

static unsigned int
agroup_add(int proxy_id, std::string & dbname, std::string & pattern)
{
    //char q[pattern.length() + 1024];
    char * q = new char [ pattern.length() + 1024 ];
    GreenSQLConfig * conf = GreenSQLConfig::getInstance();
    MYSQL * dbConn = &conf->dbConn;

    snprintf(q, pattern.length() + 1024,
        "INSERT into alert_group "
                "(proxyid, db_name, update_time, pattern) "
                "VALUES (%d,'%s',now(),'%s')",
                proxy_id, dbname.c_str(), pattern.c_str());

    /* read new urls from the database */
    if( mysql_query(dbConn, q) )
    {
        /* Make query */
        logevent(STORAGE,"(%s) %s\n", q, mysql_error(dbConn));
        delete [] q;
        return 0;
    }
    delete [] q;
    q = NULL;
    if (!mysql_affected_rows(dbConn) )
    {
        return 0;
    }
    return agroup_get(proxy_id, dbname, pattern);
}

static int alert_add(unsigned int agroupid, char * user,
        char * query, char * reason, int risk, int block)
{
    //char q[strlen(query) + strlen(reason) + 1024];
    char * q = new char [ strlen(query) + strlen(reason) + 1024 ];
    GreenSQLConfig * conf = GreenSQLConfig::getInstance();
    MYSQL * dbConn = &conf->dbConn;

    snprintf(q, strlen(query) + strlen(reason) + 1024,
                "INSERT into alert "
                "(agroupid, event_time, risk, block, user, query, reason) "
                "VALUES (%d,now(),%d,%d,'%s','%s','%s')",
                agroupid, risk, block, user, query, reason);

    /* read new urls from the database */
    if( mysql_query(dbConn, q) )
    {
        /* Make query */
        logevent(STORAGE,"(%s) %s\n", q, mysql_error(dbConn));
        delete [] q;
        return 0;
    }
    delete [] q;
    q = NULL;
    if (!mysql_affected_rows(dbConn) )
    {
        return 0;
    }
    return 1; 
}

static bool
agroup_update(unsigned int agroupid)
{
    char q[1024];
    GreenSQLConfig * conf = GreenSQLConfig::getInstance();
    MYSQL * dbConn = &conf->dbConn;

    snprintf(q, sizeof(q), 
               "UPDATE alert_group SET update_time=now() WHERE agroupid = %u",
               agroupid);

    /* read new urls from the database */
    if( mysql_query(dbConn, q) )
    {
        /* Make query */
        logevent(STORAGE,"(%s) %s\n", q, mysql_error(dbConn));
        return false;
    }
    if (!mysql_affected_rows(dbConn) )
    {
        return false;
    }
    return true;
}


static bool
agroup_update_status(unsigned int agroupid)
{
    char q[1024];
    GreenSQLConfig * conf = GreenSQLConfig::getInstance();
    MYSQL * dbConn = &conf->dbConn;

    snprintf(q, sizeof(q),
               "UPDATE alert_group SET update_time=now(), status=1 WHERE agroupid = %u",
               agroupid);

    /* read new urls from the database */
    if( mysql_query(dbConn, q) )
    {
        /* Make query */
        logevent(STORAGE,"(%s) %s\n", q, mysql_error(dbConn));
        return false;
    }
    if (!mysql_affected_rows(dbConn) )
    {
        return false;
    }
    return true;
}
