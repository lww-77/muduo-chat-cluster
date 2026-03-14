#include"groupmodel.hpp"
#include"db.h"

    //创建群组
    bool GroupModel::creatGroup(Group& group)
    {
        //组装SQL语句
        char sql[1024]={0};
        sprintf(sql, "insert into allgroup (groupname, groupdesc) values('%s', '%s')",
                group.getGroupName().c_str(),group.getGroupDesc().c_str());

        MySQL mysql;
        if(mysql.connect())
        {
            if(mysql.update(sql))
            {
                //获取插入成功组生成的id
                group.setGroupId(mysql_insert_id(mysql.getConnection()));
                return true;
            }
        }
        return false;

    }

    //加入群组
    void GroupModel::joinGroup(int userid,int groupid,string role)
    {
        //组装SQL语句
        char sql[1024]={0};
        sprintf(sql, "insert into groupuser (groupid, userid, grouprole) values(%d,%d,'%s')",
                groupid, userid, role.c_str());

        MySQL mysql;
        if(mysql.connect())
        {
            mysql.update(sql);

        }
    }

    //查询用户所在群组信息以及群组内用户的信息
    vector<Group> GroupModel::queryGroup(int userid)
    {

        /*
        1、根据userid在groupuser表中查询出该用户所属群组的信息
        2、再根据群组信息，查询属于该群组的所有用户的userid,并且和user表进行多表联合查询，查出用户的详细信息
        */
        //组装SQL语句
        char sql[1024]={0};
        sprintf(sql, "select a.id,a.groupname,a.groupdesc from allgroup a inner join groupuser b on a.id = b.groupid where b.userid = %d",
                userid);
        
        vector<Group>vec;
        MySQL mysql;
        if(mysql.connect())
        {
            MYSQL_RES* res = mysql.query(sql);
            if(res!=nullptr)
            {
                MYSQL_ROW row;
                while((row=mysql_fetch_row(res))!=nullptr)
                {
                    Group group;
                    group.setGroupId(atoi(row[0]));
                    group.setGroupName(row[1]);
                    group.setGroupDesc(row[2]);
                    vec.push_back(group);
                }
                mysql_free_result(res);
            }

        }
        //查询群组的用户信息

        for(Group& group:vec)
        {
            sprintf(sql,"select a.id,a.name,a.state,b.grouprole from user a inner join groupuser b on b.userid = a.id where b.groupid = %d"
                    ,group.getGroupId());



            if(mysql.connect())
            {
                MYSQL_RES* res = mysql.query(sql);
                if(res!=nullptr)
                {
                    MYSQL_ROW row;
                    while((row=mysql_fetch_row(res))!=nullptr)
                    {
                        GroupUser groupuser;
                        groupuser.setId(atoi(row[0]));
                        groupuser.setName(row[1]);
                        groupuser.setState(row[2]);
                        groupuser.setRole(row[3]);
                        group.getGroupUsers().push_back(groupuser);
                    }
                    mysql_free_result(res);
                }

            }

        }
        return vec;
    }

    //根据指定的groupid查询群组用户id列表，除userid自己，主要用户群聊业务给群组其他成员群发消息
    vector<int> GroupModel::queryGroupUser(int userid, int groupid)
    {
            //组装SQL语句
        char sql[1024]={0};
        sprintf(sql, "select userid from groupuser where groupid = %d and userid != %d",
                groupid, userid);
        
        vector<int>useridvec;
        MySQL mysql;
        if(mysql.connect())
        {
            MYSQL_RES* res = mysql.query(sql);
            if(res!=nullptr)
            {
                MYSQL_ROW row;
                while((row=mysql_fetch_row(res))!=nullptr)
                {
                    useridvec.push_back(atoi(row[0]));
                }
                mysql_free_result(res);

            }

        }
        return useridvec;

    }