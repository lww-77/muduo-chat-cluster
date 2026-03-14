#ifndef GROUP_H
#define GROUP_H


#include<string>
#include<vector>
#include"groupuser.hpp"
using namespace std;



class Group
{
public:

    Group(int id=-1,string name="",string desc="")
    {
        this->_groupid=id;
        this->_groupname=name;
        this->_groupDesc=desc;
    }

    void setGroupId(int groupid){this->_groupid=groupid;}
    void setGroupName(string groupname){this->_groupname=groupname;}
    void setGroupDesc(string groupDesc){this->_groupDesc=groupDesc;}

    int getGroupId(){return this->_groupid;}
    string getGroupName(){return this->_groupname;}
    string getGroupDesc(){return this->_groupDesc;}
    vector<GroupUser>& getGroupUsers(){return this->_groupusers;}
private:
    int _groupid;
    string _groupname;
    string _groupDesc;
    vector<GroupUser> _groupusers;
};




#endif