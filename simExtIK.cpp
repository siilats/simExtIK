#include "simExtIK.h"
#include "envCont.h"
#include "simLib.h"
#include "ik.h"
#include "4X4Matrix.h"
#include <iostream>
#include <cstdio>
#include "scriptFunctionData.h"
#include <algorithm>

#ifdef _WIN32
#ifdef QT_COMPIL
    #include <direct.h>
#else
    #include <shlwapi.h>
    #pragma comment(lib, "Shlwapi.lib")
#endif
#endif
#ifdef _WIN32
    #include <Windows.h>
    typedef CRITICAL_SECTION WMutex;
#endif
#if defined (__linux) || defined (__APPLE__)
    #include <unistd.h>
    #include <pthread.h>
    typedef pthread_mutex_t WMutex;
#endif

#define CONCAT(x,y,z) x y z
#define strConCat(x,y,z)    CONCAT(x,y,z)

static LIBRARY simLib;
static WMutex _simpleMutex;
static CEnvCont* _allEnvironments;

void lockInterface()
{
    #ifdef _WIN32
        EnterCriticalSection(&_simpleMutex);
    #endif
    #ifdef __APPLE__
        while (pthread_mutex_lock(&_simpleMutex)==-1)
            pthread_yield_np();
    #endif
    #ifdef __linux
        while (pthread_mutex_lock(&_simpleMutex)==-1)
            pthread_yield();
    #endif
}

void unlockInterface()
{
    #ifdef _WIN32
        LeaveCriticalSection(&_simpleMutex);
    #else
        pthread_mutex_unlock(&_simpleMutex);
    #endif
}

class CLockInterface
{
public:
    CLockInterface()
    {
        lockInterface();
    };
    virtual ~CLockInterface()
    {
        unlockInterface();
    };
};

bool _logCallback(int verbosity,const char* funcName,const char* msg)
{
    bool retVal=true;
    int v=sim_verbosity_none;
    if (verbosity==1)
        v=sim_verbosity_scripterrors;
    if (verbosity==2)
        v=sim_verbosity_scriptwarnings;
    if (verbosity==3)
        v=sim_verbosity_scriptinfos;
    if (verbosity==4)
        v=sim_verbosity_debug;
    if (verbosity==5)
        v=sim_verbosity_trace;
    if (verbosity!=1)
    {
        std::string m(funcName);
        m+=": ";
        m+=msg;
        simAddLog("IK",v,m.c_str());
    }
    else
        retVal=false;
    return(retVal);
}

// --------------------------------------------------------------------------------------
// simIK.createEnvironment
// --------------------------------------------------------------------------------------
#define LUA_CREATEENVIRONMENT_COMMAND_PLUGIN "simIK.createEnvironment@IK"
#define LUA_CREATEENVIRONMENT_COMMAND "simIK.createEnvironment"

const int inArgs_CREATEENVIRONMENT[]={
    0,
};

void LUA_CREATEENVIRONMENT_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int retVal=-1;
    bool res=false;
    if (D.readDataFromStack(p->stackID,inArgs_CREATEENVIRONMENT,inArgs_CREATEENVIRONMENT[0],LUA_CREATEENVIRONMENT_COMMAND))
    {
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            res=ikCreateEnvironment(&retVal);
            if (res)
                _allEnvironments->add(retVal,p->scriptID);
            else
                err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_CREATEENVIRONMENT_COMMAND,err.c_str());
    }
    if (res)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK._eraseEnvironment
// --------------------------------------------------------------------------------------
#define LUA_ERASEENVIRONMENT_COMMAND_PLUGIN "simIK._eraseEnvironment@IK"
#define LUA_ERASEENVIRONMENT_COMMAND "simIK._eraseEnvironment"

const int inArgs_ERASEENVIRONMENT[]={
    1,
    sim_script_arg_int32,0,
};

void LUA_ERASEENVIRONMENT_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_ERASEENVIRONMENT,inArgs_ERASEENVIRONMENT[0],LUA_ERASEENVIRONMENT_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                if (ikEraseEnvironment())
                    _allEnvironments->removeFromEnvHandle(envId);
                else
                    err=ikGetLastError();
            }
            else
                err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_ERASEENVIRONMENT_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.duplicateEnvironment
// --------------------------------------------------------------------------------------
#define LUA_DUPLICATEENVIRONMENT_COMMAND_PLUGIN "simIK.duplicateEnvironment@IK"
#define LUA_DUPLICATEENVIRONMENT_COMMAND "simIK.duplicateEnvironment"

const int inArgs_DUPLICATEENVIRONMENT[]={
    1,
    sim_script_arg_int32,0,
};

void LUA_DUPLICATEENVIRONMENT_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int retVal=-1;
    bool res=false;
    if (D.readDataFromStack(p->stackID,inArgs_DUPLICATEENVIRONMENT,inArgs_DUPLICATEENVIRONMENT[0],LUA_DUPLICATEENVIRONMENT_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                if (ikDuplicateEnvironment(&retVal))
                {
                    _allEnvironments->add(retVal,p->scriptID);
                    res=true;
                }
                else
                    err=ikGetLastError();
            }
            else
                err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_DUPLICATEENVIRONMENT_COMMAND,err.c_str());
    }
    if (res)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.load
// --------------------------------------------------------------------------------------
#define LUA_LOAD_COMMAND_PLUGIN "simIK.load@IK"
#define LUA_LOAD_COMMAND "simIK.load"

const int inArgs_LOAD[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_charbuff,0,
};

void LUA_LOAD_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_LOAD,inArgs_LOAD[0],LUA_LOAD_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        std::string buff(inData->at(1).stringData[0]);
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                if (!ikLoad((unsigned char*)buff.c_str(),buff.length()))
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_LOAD_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.save
// --------------------------------------------------------------------------------------
#define LUA_SAVE_COMMAND_PLUGIN "simIK.save@IK"
#define LUA_SAVE_COMMAND "simIK.save"

const int inArgs_SAVE[]={
    1,
    sim_script_arg_int32,0,
};

void LUA_SAVE_CALLBACK(SScriptCallBack* p)
{
    std::string retVal;
    bool res=false;
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SAVE,inArgs_SAVE[0],LUA_SAVE_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];

        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                size_t l;
                unsigned char* data=ikSave(&l);
                if (data!=nullptr)
                {
                    res=true;
                    retVal.assign(data,data+l);
                }
                else
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SAVE_COMMAND,err.c_str());
    }
    if (res)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getObjectHandle
// --------------------------------------------------------------------------------------
#define LUA_GETOBJECTHANDLE_COMMAND_PLUGIN "simIK.getObjectHandle@IK"
#define LUA_GETOBJECTHANDLE_COMMAND "simIK.getObjectHandle"

const int inArgs_GETOBJECTHANDLE[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_string,0,
};

void LUA_GETOBJECTHANDLE_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int retVal=-1;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETOBJECTHANDLE,inArgs_GETOBJECTHANDLE[0],LUA_GETOBJECTHANDLE_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetObjectHandle(inData->at(1).stringData[0].c_str(),&retVal);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETOBJECTHANDLE_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.doesObjectExist
// --------------------------------------------------------------------------------------
#define LUA_DOESOBJECTEXIST_COMMAND_PLUGIN "simIK.doesObjectExist@IK"
#define LUA_DOESOBJECTEXIST_COMMAND "simIK.doesObjectExist"

const int inArgs_DOESOBJECTEXIST[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_string,0,
};

void LUA_DOESOBJECTEXIST_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    bool retVal=false;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_DOESOBJECTEXIST,inArgs_DOESOBJECTEXIST[0],LUA_DOESOBJECTEXIST_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                retVal=ikDoesObjectExist(inData->at(1).stringData[0].c_str());
                result=true;
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_DOESOBJECTEXIST_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.eraseObject
// --------------------------------------------------------------------------------------
#define LUA_ERASEOBJECT_COMMAND_PLUGIN "simIK.eraseObject@IK"
#define LUA_ERASEOBJECT_COMMAND "simIK.eraseObject"

const int inArgs_ERASEOBJECT[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_ERASEOBJECT_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_ERASEOBJECT,inArgs_ERASEOBJECT[0],LUA_ERASEOBJECT_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int objectHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                if (!ikEraseObject(objectHandle))
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_ERASEOBJECT_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getObjectParent
// --------------------------------------------------------------------------------------
#define LUA_GETOBJECTPARENT_COMMAND_PLUGIN "simIK.getObjectParent@IK"
#define LUA_GETOBJECTPARENT_COMMAND "simIK.getObjectParent"

const int inArgs_GETOBJECTPARENT[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETOBJECTPARENT_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int retVal=-1;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETOBJECTPARENT,inArgs_GETOBJECTPARENT[0],LUA_GETOBJECTPARENT_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int objectHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetObjectParent(objectHandle,&retVal);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETOBJECTPARENT_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setObjectParent
// --------------------------------------------------------------------------------------
#define LUA_SETOBJECTPARENT_COMMAND_PLUGIN "simIK.setObjectParent@IK"
#define LUA_SETOBJECTPARENT_COMMAND "simIK.setObjectParent"

const int inArgs_SETOBJECTPARENT[]={
    4,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_bool,0,
};

void LUA_SETOBJECTPARENT_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETOBJECTPARENT,inArgs_SETOBJECTPARENT[0]-1,LUA_SETOBJECTPARENT_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int objectHandle=inData->at(1).int32Data[0];
        int parentObjectHandle=inData->at(2).int32Data[0];
        bool keepInPlace=true;
        if ( (inData->size()>3)&&(inData->at(3).boolData.size()==1) )
            keepInPlace=inData->at(3).boolData[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetObjectParent(objectHandle,parentObjectHandle,keepInPlace);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETOBJECTPARENT_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getObjects
// --------------------------------------------------------------------------------------
#define LUA_GETOBJECTS_COMMAND_PLUGIN "simIK.getObjects@IK"
#define LUA_GETOBJECTS_COMMAND "simIK.getObjects"

const int inArgs_GETOBJECTS[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETOBJECTS_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int objectHandle=-1;
    std::string objectName;
    bool isJoint=false;
    int jointType=ik_jointtype_revolute;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETOBJECTS,inArgs_GETOBJECTS[0],LUA_GETOBJECTS_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int index=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetObjects(size_t(index),&objectHandle,&objectName,&isJoint,&jointType);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETOBJECTS_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(objectHandle));
        D.pushOutData(CScriptFunctionDataItem(objectName));
        D.pushOutData(CScriptFunctionDataItem(isJoint));
        D.pushOutData(CScriptFunctionDataItem(jointType));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.createDummy
// --------------------------------------------------------------------------------------
#define LUA_CREATEDUMMY_COMMAND_PLUGIN "simIK.createDummy@IK"
#define LUA_CREATEDUMMY_COMMAND "simIK.createDummy"

const int inArgs_CREATEDUMMY[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_string|SIM_SCRIPT_ARG_NULL_ALLOWED,0,
};

void LUA_CREATEDUMMY_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int retVal=-1;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_CREATEDUMMY,inArgs_CREATEDUMMY[0]-1,LUA_CREATEDUMMY_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                const char* nm=nullptr;
                if ( (inData->size()>1)&&(inData->at(1).stringData.size()==1)&&(inData->at(1).stringData[0].size()>0) )
                    nm=inData->at(1).stringData[0].c_str();
                result=ikCreateDummy(nm,&retVal);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_CREATEDUMMY_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getLinkedDummy
// --------------------------------------------------------------------------------------
#define LUA_GETLINKEDDUMMY_COMMAND_PLUGIN "simIK.getLinkedDummy@IK"
#define LUA_GETLINKEDDUMMY_COMMAND "simIK.getLinkedDummy"

const int inArgs_GETLINKEDDUMMY[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETLINKEDDUMMY_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int retVal=-1;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETLINKEDDUMMY,inArgs_GETLINKEDDUMMY[0],LUA_GETLINKEDDUMMY_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int dummyHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetLinkedDummy(dummyHandle,&retVal);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETLINKEDDUMMY_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setLinkedDummy
// --------------------------------------------------------------------------------------
#define LUA_SETLINKEDDUMMY_COMMAND_PLUGIN "simIK.setLinkedDummy@IK"
#define LUA_SETLINKEDDUMMY_COMMAND "simIK.setLinkedDummy"

const int inArgs_SETLINKEDDUMMY[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_SETLINKEDDUMMY_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETLINKEDDUMMY,inArgs_SETLINKEDDUMMY[0],LUA_SETLINKEDDUMMY_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int dummyHandle=inData->at(1).int32Data[0];
        int linkedDummyHandle=inData->at(2).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetLinkedDummy(dummyHandle,linkedDummyHandle);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETLINKEDDUMMY_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.createJoint
// --------------------------------------------------------------------------------------
#define LUA_CREATEJOINT_COMMAND_PLUGIN "simIK.createJoint@IK"
#define LUA_CREATEJOINT_COMMAND "simIK.createJoint"

const int inArgs_CREATEJOINT[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_string|SIM_SCRIPT_ARG_NULL_ALLOWED,0,
};

void LUA_CREATEJOINT_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int retVal=-1;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_CREATEJOINT,inArgs_CREATEJOINT[0]-1,LUA_CREATEJOINT_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                int jType=inData->at(1).int32Data[0];
                const char* nm=nullptr;
                if ( (inData->size()>2)&&(inData->at(2).stringData.size()==1)&&(inData->at(2).stringData[0].size()>0) )
                    nm=inData->at(2).stringData[0].c_str();
                result=ikCreateJoint(nm,jType,&retVal);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_CREATEJOINT_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getJointType
// --------------------------------------------------------------------------------------
#define LUA_GETJOINTTYPE_COMMAND_PLUGIN "simIK.getJointType@IK"
#define LUA_GETJOINTTYPE_COMMAND "simIK.getJointType"

const int inArgs_GETJOINTTYPE[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETJOINTTYPE_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int retVal=-1;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETJOINTTYPE,inArgs_GETJOINTTYPE[0],LUA_GETJOINTTYPE_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetJointType(jointHandle,&retVal);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETJOINTTYPE_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getJointMode
// --------------------------------------------------------------------------------------
#define LUA_GETJOINTMODE_COMMAND_PLUGIN "simIK.getJointMode@IK"
#define LUA_GETJOINTMODE_COMMAND "simIK.getJointMode"

const int inArgs_GETJOINTMODE[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETJOINTMODE_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int retVal=-1;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETJOINTMODE,inArgs_GETJOINTMODE[0],LUA_GETJOINTMODE_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetJointMode(jointHandle,&retVal);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETJOINTMODE_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setJointMode
// --------------------------------------------------------------------------------------
#define LUA_SETJOINTMODE_COMMAND_PLUGIN "simIK.setJointMode@IK"
#define LUA_SETJOINTMODE_COMMAND "simIK.setJointMode"

const int inArgs_SETJOINTMODE[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_SETJOINTMODE_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETJOINTMODE,inArgs_SETJOINTMODE[0],LUA_SETJOINTMODE_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        int jointMode=inData->at(2).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetJointMode(jointHandle,jointMode);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETJOINTMODE_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getJointInterval
// --------------------------------------------------------------------------------------
#define LUA_GETJOINTINTERVAL_COMMAND_PLUGIN "simIK.getJointInterval@IK"
#define LUA_GETJOINTINTERVAL_COMMAND "simIK.getJointInterval"

const int inArgs_GETJOINTINTERVAL[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETJOINTINTERVAL_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    bool cyclic=true;
    double interv[2];
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETJOINTINTERVAL,inArgs_GETJOINTINTERVAL[0],LUA_GETJOINTINTERVAL_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetJointInterval(jointHandle,&cyclic,interv);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETJOINTINTERVAL_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(cyclic));
        std::vector<double> iv(interv,interv+2);
        D.pushOutData(iv);
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setJointInterval
// --------------------------------------------------------------------------------------
#define LUA_SETJOINTINTERVAL_COMMAND_PLUGIN "simIK.setJointInterval@IK"
#define LUA_SETJOINTINTERVAL_COMMAND "simIK.setJointInterval"

const int inArgs_SETJOINTINTERVAL[]={
    4,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_bool,0,
    sim_script_arg_double|sim_script_arg_table,2,
};

void LUA_SETJOINTINTERVAL_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETJOINTINTERVAL,inArgs_SETJOINTINTERVAL[0]-1,LUA_SETJOINTINTERVAL_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        bool cyclic=inData->at(2).boolData[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                double* interv=nullptr;
                if ( (inData->size()>3)&&(inData->at(3).doubleData.size()>=2) )
                    interv=&inData->at(3).doubleData[0];

                bool result=ikSetJointInterval(jointHandle,cyclic,interv);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETJOINTINTERVAL_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getJointScrewPitch
// --------------------------------------------------------------------------------------
#define LUA_GETJOINTSCREWPITCH_COMMAND_PLUGIN "simIK.getJointScrewPitch@IK"
#define LUA_GETJOINTSCREWPITCH_COMMAND "simIK.getJointScrewPitch"

const int inArgs_GETJOINTSCREWPITCH[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETJOINTSCREWPITCH_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    double pitch=0.0;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETJOINTSCREWPITCH,inArgs_GETJOINTSCREWPITCH[0],LUA_GETJOINTSCREWPITCH_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetJointScrewPitch(jointHandle,&pitch);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETJOINTSCREWPITCH_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(pitch));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setJointScrewPitch
// --------------------------------------------------------------------------------------
#define LUA_SETJOINTSCREWPITCH_COMMAND_PLUGIN "simIK.setJointScrewPitch@IK"
#define LUA_SETJOINTSCREWPITCH_COMMAND "simIK.setJointScrewPitch"

const int inArgs_SETJOINTSCREWPITCH[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_double,0,
};

void LUA_SETJOINTSCREWPITCH_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETJOINTSCREWPITCH,inArgs_SETJOINTSCREWPITCH[0],LUA_SETJOINTSCREWPITCH_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        double pitch=inData->at(2).doubleData[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetJointScrewPitch(jointHandle,pitch);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETJOINTSCREWPITCH_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getJointIkWeight
// --------------------------------------------------------------------------------------
#define LUA_GETJOINTIKWEIGHT_COMMAND_PLUGIN "simIK.getJointIkWeight@IK"
#define LUA_GETJOINTIKWEIGHT_COMMAND "simIK.getJointIkWeight"

const int inArgs_GETJOINTIKWEIGHT[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETJOINTIKWEIGHT_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    double weight=1.0;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETJOINTIKWEIGHT,inArgs_GETJOINTIKWEIGHT[0],LUA_GETJOINTIKWEIGHT_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetJointIkWeight(jointHandle,&weight);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETJOINTIKWEIGHT_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(weight));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setJointIkWeight
// --------------------------------------------------------------------------------------
#define LUA_SETJOINTIKWEIGHT_COMMAND_PLUGIN "simIK.setJointIkWeight@IK"
#define LUA_SETJOINTIKWEIGHT_COMMAND "simIK.setJointIkWeight"

const int inArgs_SETJOINTIKWEIGHT[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_double,0,
};

void LUA_SETJOINTIKWEIGHT_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETJOINTIKWEIGHT,inArgs_SETJOINTIKWEIGHT[0],LUA_SETJOINTIKWEIGHT_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        double weight=inData->at(2).doubleData[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetJointIkWeight(jointHandle,weight);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETJOINTIKWEIGHT_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getJointLimitMargin
// --------------------------------------------------------------------------------------
#define LUA_GETJOINTLIMITMARGIN_COMMAND_PLUGIN "simIK.getJointLimitMargin@IK"
#define LUA_GETJOINTLIMITMARGIN_COMMAND "simIK.getJointLimitMargin"

const int inArgs_GETJOINTLIMITMARGIN[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETJOINTLIMITMARGIN_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    double weight=1.0;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETJOINTLIMITMARGIN,inArgs_GETJOINTLIMITMARGIN[0],LUA_GETJOINTLIMITMARGIN_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetJointLimitMargin(jointHandle,&weight);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETJOINTLIMITMARGIN_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(weight));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setJointLimitMargin
// --------------------------------------------------------------------------------------
#define LUA_SETJOINTLIMITMARGIN_COMMAND_PLUGIN "simIK.setJointLimitMargin@IK"
#define LUA_SETJOINTLIMITMARGIN_COMMAND "simIK.setJointLimitMargin"

const int inArgs_SETJOINTLIMITMARGIN[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_double,0,
};

void LUA_SETJOINTLIMITMARGIN_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETJOINTLIMITMARGIN,inArgs_SETJOINTLIMITMARGIN[0],LUA_SETJOINTLIMITMARGIN_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        double weight=inData->at(2).doubleData[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetJointLimitMargin(jointHandle,weight);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETJOINTLIMITMARGIN_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getJointMaxStepSize
// --------------------------------------------------------------------------------------
#define LUA_GETJOINTMAXSTEPSIZE_COMMAND_PLUGIN "simIK.getJointMaxStepSize@IK"
#define LUA_GETJOINTMAXSTEPSIZE_COMMAND "simIK.getJointMaxStepSize"

const int inArgs_GETJOINTMAXSTEPSIZE[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETJOINTMAXSTEPSIZE_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    double stepSize=0.0;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETJOINTMAXSTEPSIZE,inArgs_GETJOINTMAXSTEPSIZE[0],LUA_GETJOINTMAXSTEPSIZE_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetJointMaxStepSize(jointHandle,&stepSize);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETJOINTMAXSTEPSIZE_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(stepSize));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setJointMaxStepSize
// --------------------------------------------------------------------------------------
#define LUA_SETJOINTMAXSTEPSIZE_COMMAND_PLUGIN "simIK.setJointMaxStepSize@IK"
#define LUA_SETJOINTMAXSTEPSIZE_COMMAND "simIK.setJointMaxStepSize"

const int inArgs_SETJOINTMAXSTEPSIZE[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_double,0,
};

void LUA_SETJOINTMAXSTEPSIZE_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETJOINTMAXSTEPSIZE,inArgs_SETJOINTMAXSTEPSIZE[0],LUA_SETJOINTMAXSTEPSIZE_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        double stepSize=inData->at(2).doubleData[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetJointMaxStepSize(jointHandle,stepSize);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETJOINTMAXSTEPSIZE_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getJointDependency
// --------------------------------------------------------------------------------------
#define LUA_GETJOINTDEPENDENCY_COMMAND_PLUGIN "simIK.getJointDependency@IK"
#define LUA_GETJOINTDEPENDENCY_COMMAND "simIK.getJointDependency"

const int inArgs_GETJOINTDEPENDENCY[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETJOINTDEPENDENCY_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int depJoint=-1;
    double offset=0.0;
    double mult=1.0;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETJOINTDEPENDENCY,inArgs_GETJOINTDEPENDENCY[0],LUA_GETJOINTDEPENDENCY_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetJointDependency(jointHandle,&depJoint,&offset,&mult);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETJOINTDEPENDENCY_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(depJoint));
        if (depJoint!=-1)
        {
            D.pushOutData(CScriptFunctionDataItem(offset));
            D.pushOutData(CScriptFunctionDataItem(mult));
        }
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setJointDependency
// --------------------------------------------------------------------------------------
#define LUA_SETJOINTDEPENDENCY_COMMAND_PLUGIN "simIK.setJointDependency@IK"
#define LUA_SETJOINTDEPENDENCY_COMMAND "simIK.setJointDependency"

const int inArgs_SETJOINTDEPENDENCY[]={
    5,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_double,0,
    sim_script_arg_double,0,
};

void LUA_SETJOINTDEPENDENCY_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETJOINTDEPENDENCY,inArgs_SETJOINTDEPENDENCY[0]-2,LUA_SETJOINTDEPENDENCY_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        int depJointHandle=inData->at(2).int32Data[0];
        double off=0.0;
        double mult=1.0;
        if ( (inData->size()>3)&&(inData->at(3).doubleData.size()==1) )
            off=inData->at(3).doubleData[0];
        if ( (inData->size()>4)&&(inData->at(4).doubleData.size()==1) )
            mult=inData->at(4).doubleData[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetJointDependency(jointHandle,depJointHandle,off,mult);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETJOINTDEPENDENCY_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getJointPosition
// --------------------------------------------------------------------------------------
#define LUA_GETJOINTPOSITION_COMMAND_PLUGIN "simIK.getJointPosition@IK"
#define LUA_GETJOINTPOSITION_COMMAND "simIK.getJointPosition"

const int inArgs_GETJOINTPOSITION[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETJOINTPOSITION_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    double pos=0.0;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETJOINTPOSITION,inArgs_GETJOINTPOSITION[0],LUA_GETJOINTPOSITION_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetJointPosition(jointHandle,&pos);
                if (!result)
                    err=ikGetLastError();
            }
            else
                err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETJOINTPOSITION_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(pos));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setJointPosition
// --------------------------------------------------------------------------------------
#define LUA_SETJOINTPOSITION_COMMAND_PLUGIN "simIK.setJointPosition@IK"
#define LUA_SETJOINTPOSITION_COMMAND "simIK.setJointPosition"

const int inArgs_SETJOINTPOSITION[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_double,0,
};

void LUA_SETJOINTPOSITION_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETJOINTPOSITION,inArgs_SETJOINTPOSITION[0],LUA_SETJOINTPOSITION_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        double pos=inData->at(2).doubleData[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetJointPosition(jointHandle,pos);
                if (!result)
                    err=ikGetLastError();
            }
            else
                err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETJOINTPOSITION_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getJointMatrix
// --------------------------------------------------------------------------------------
#define LUA_GETJOINTMATRIX_COMMAND_PLUGIN "simIK.getJointMatrix@IK"
#define LUA_GETJOINTMATRIX_COMMAND "simIK.getJointMatrix"

const int inArgs_GETJOINTMATRIX[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETJOINTMATRIX_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    double matrix[12];
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETJOINTMATRIX,inArgs_GETJOINTMATRIX[0],LUA_GETJOINTMATRIX_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                C7Vector tr;
                result=ikGetJointTransformation(jointHandle,&tr);
                if (result)
                    tr.getMatrix().getData(matrix);
                else
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETJOINTMATRIX_COMMAND,err.c_str());
    }
    if (result)
    {
        std::vector<double> m(matrix,matrix+12);
        D.pushOutData(CScriptFunctionDataItem(m));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setSphericalJointMatrix
// --------------------------------------------------------------------------------------
#define LUA_SETSPHERICALJOINTMATRIX_COMMAND_PLUGIN "simIK.setSphericalJointMatrix@IK"
#define LUA_SETSPHERICALJOINTMATRIX_COMMAND "simIK.setSphericalJointMatrix"

const int inArgs_SETSPHERICALJOINTMATRIX[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_double|sim_script_arg_table,12,
};

void LUA_SETSPHERICALJOINTMATRIX_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETSPHERICALJOINTMATRIX,inArgs_SETSPHERICALJOINTMATRIX[0],LUA_SETSPHERICALJOINTMATRIX_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        double* m=&inData->at(2).doubleData[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                C4X4Matrix _m;
                _m.setData(m);
                C4Vector q(_m.M.getQuaternion());
                bool result=ikSetSphericalJointQuaternion(jointHandle,&q);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETSPHERICALJOINTMATRIX_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getJointTransformation
// --------------------------------------------------------------------------------------
#define LUA_GETJOINTTRANSFORMATION_COMMAND_PLUGIN "simIK.getJointTransformation@IK"
#define LUA_GETJOINTTRANSFORMATION_COMMAND "simIK.getJointTransformation"

const int inArgs_GETJOINTTRANSFORMATION[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETJOINTTRANSFORMATION_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    double pos[3];
    double quat[4];
    double e[3];
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETJOINTTRANSFORMATION,inArgs_GETJOINTTRANSFORMATION[0],LUA_GETJOINTTRANSFORMATION_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                C7Vector tr;
                result=ikGetJointTransformation(jointHandle,&tr);
                if (result)
                {   // CoppeliaSim quaternion, internally: w x y z
                    // CoppeliaSim quaternion, at interfaces: x y z w
                    tr.X.getData(pos);
                    quat[0]=tr.Q(1);
                    quat[1]=tr.Q(2);
                    quat[2]=tr.Q(3);
                    quat[3]=tr.Q(0);
                    tr.Q.getEulerAngles().getData(e);
                }
                else
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETJOINTTRANSFORMATION_COMMAND,err.c_str());
    }
    if (result)
    {
        std::vector<double> _p(pos,pos+3);
        D.pushOutData(CScriptFunctionDataItem(_p));
        std::vector<double> _q(quat,quat+4);
        D.pushOutData(CScriptFunctionDataItem(_q));
        std::vector<double> _e(e,e+3);
        D.pushOutData(CScriptFunctionDataItem(_e));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setSphericalJointRotation
// --------------------------------------------------------------------------------------
#define LUA_SETSPHERICALJOINTROTATION_COMMAND_PLUGIN "simIK.setSphericalJointRotation@IK"
#define LUA_SETSPHERICALJOINTROTATION_COMMAND "simIK.setSphericalJointRotation"

const int inArgs_SETSPHERICALJOINTROTATION[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_double|sim_script_arg_table,3,
};

void LUA_SETSPHERICALJOINTROTATION_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETSPHERICALJOINTROTATION,inArgs_SETSPHERICALJOINTROTATION[0],LUA_SETSPHERICALJOINTROTATION_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int jointHandle=inData->at(1).int32Data[0];
        double* quat=nullptr;
        double* euler=nullptr;
        if (inData->at(2).doubleData.size()==3)
            euler=&inData->at(2).doubleData[0];
        else
            quat=&inData->at(2).doubleData[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {   // CoppeliaSim quaternion, internally: w x y z
                // CoppeliaSim quaternion, at interfaces: x y z w
                C4Vector q;
                if (euler!=nullptr)
                    q.setEulerAngles(C3Vector(euler));
                else
                    q=C4Vector(quat[3],quat[0],quat[1],quat[2]);
                bool result=ikSetSphericalJointQuaternion(jointHandle,&q);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETSPHERICALJOINTROTATION_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getIkGroupHandle
// --------------------------------------------------------------------------------------
#define LUA_GETIKGROUPHANDLE_COMMAND_PLUGIN "simIK.getIkGroupHandle@IK"
#define LUA_GETIKGROUPHANDLE_COMMAND "simIK.getIkGroupHandle"

const int inArgs_GETIKGROUPHANDLE[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_string,0,
};

void LUA_GETIKGROUPHANDLE_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int retVal=-1;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETIKGROUPHANDLE,inArgs_GETIKGROUPHANDLE[0],LUA_GETIKGROUPHANDLE_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetIkGroupHandle(inData->at(1).stringData[0].c_str(),&retVal);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETIKGROUPHANDLE_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.doesIkGroupExist
// --------------------------------------------------------------------------------------
#define LUA_DOESIKGROUPEXIST_COMMAND_PLUGIN "simIK.doesIkGroupExist@IK"
#define LUA_DOESIKGROUPEXIST_COMMAND "simIK.doesIkGroupExist"

const int inArgs_DOESIKGROUPEXIST[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_string,0,
};

void LUA_DOESIKGROUPEXIST_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    bool retVal=false;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_DOESIKGROUPEXIST,inArgs_DOESIKGROUPEXIST[0],LUA_DOESIKGROUPEXIST_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                retVal=ikDoesIkGroupExist(inData->at(1).stringData[0].c_str());
                result=true;
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_DOESIKGROUPEXIST_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.createIkGroup
// --------------------------------------------------------------------------------------
#define LUA_CREATEIKGROUP_COMMAND_PLUGIN "simIK.createIkGroup@IK"
#define LUA_CREATEIKGROUP_COMMAND "simIK.createIkGroup"

const int inArgs_CREATEIKGROUP[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_string|SIM_SCRIPT_ARG_NULL_ALLOWED,0,
};

void LUA_CREATEIKGROUP_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int retVal=-1;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_CREATEIKGROUP,inArgs_CREATEIKGROUP[0]-1,LUA_CREATEIKGROUP_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                const char* nm=nullptr;
                if ( (inData->size()>1)&&(inData->at(1).stringData.size()==1)&&(inData->at(1).stringData[0].size()>0) )
                    nm=inData->at(1).stringData[0].c_str();
                result=ikCreateIkGroup(nm,&retVal);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_CREATEIKGROUP_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getIkGroupFlags
// --------------------------------------------------------------------------------------
#define LUA_GETIKGROUPFLAGS_COMMAND_PLUGIN "simIK.getIkGroupFlags@IK"
#define LUA_GETIKGROUPFLAGS_COMMAND "simIK.getIkGroupFlags"

const int inArgs_GETIKGROUPFLAGS[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETIKGROUPFLAGS_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int flags=0;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETIKGROUPFLAGS,inArgs_GETIKGROUPFLAGS[0],LUA_GETIKGROUPFLAGS_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetIkGroupFlags(ikGroupHandle,&flags);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETIKGROUPFLAGS_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(flags));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setIkGroupFlags
// --------------------------------------------------------------------------------------
#define LUA_SETIKGROUPFLAGS_COMMAND_PLUGIN "simIK.setIkGroupFlags@IK"
#define LUA_SETIKGROUPFLAGS_COMMAND "simIK.setIkGroupFlags"

const int inArgs_SETIKGROUPFLAGS[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_SETIKGROUPFLAGS_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETIKGROUPFLAGS,inArgs_SETIKGROUPFLAGS[0],LUA_SETIKGROUPFLAGS_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        int flags=inData->at(2).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetIkGroupFlags(ikGroupHandle,flags);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETIKGROUPFLAGS_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getIkGroupJointLimitHits
// --------------------------------------------------------------------------------------
#define LUA_GETIKGROUPJOINTLIMITHITS_COMMAND_PLUGIN "simIK.getIkGroupJointLimitHits@IK"
#define LUA_GETIKGROUPJOINTLIMITHITS_COMMAND "simIK.getIkGroupJointLimitHits"

const int inArgs_GETIKGROUPJOINTLIMITHITS[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETIKGROUPJOINTLIMITHITS_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    std::vector<int> handles;
    std::vector<double> overshots;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETIKGROUPJOINTLIMITHITS,inArgs_GETIKGROUPJOINTLIMITHITS[0],LUA_GETIKGROUPJOINTLIMITHITS_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetIkGroupJointLimitHits(ikGroupHandle,&handles,&overshots);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETIKGROUPJOINTLIMITHITS_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(handles));
        D.pushOutData(CScriptFunctionDataItem(overshots));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getIkGroupCalculation
// --------------------------------------------------------------------------------------
#define LUA_GETIKGROUPCALCULATION_COMMAND_PLUGIN "simIK.getIkGroupCalculation@IK"
#define LUA_GETIKGROUPCALCULATION_COMMAND "simIK.getIkGroupCalculation"

const int inArgs_GETIKGROUPCALCULATION[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETIKGROUPCALCULATION_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int method=-1;
    int iterations=0;
    double damping=0.0;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETIKGROUPCALCULATION,inArgs_GETIKGROUPCALCULATION[0],LUA_GETIKGROUPCALCULATION_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetIkGroupCalculation(ikGroupHandle,&method,&damping,&iterations);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETIKGROUPCALCULATION_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(method));
        D.pushOutData(CScriptFunctionDataItem(damping));
        D.pushOutData(CScriptFunctionDataItem(iterations));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setIkGroupCalculation
// --------------------------------------------------------------------------------------
#define LUA_SETIKGROUPCALCULATION_COMMAND_PLUGIN "simIK.setIkGroupCalculation@IK"
#define LUA_SETIKGROUPCALCULATION_COMMAND "simIK.setIkGroupCalculation"

const int inArgs_SETIKGROUPCALCULATION[]={
    5,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_double,0,
    sim_script_arg_int32,0,
};

void LUA_SETIKGROUPCALCULATION_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETIKGROUPCALCULATION,inArgs_SETIKGROUPCALCULATION[0],LUA_SETIKGROUPCALCULATION_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        int method=inData->at(2).int32Data[0];
        double damping=inData->at(3).doubleData[0];
        int iterations=inData->at(4).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetIkGroupCalculation(ikGroupHandle,method,damping,iterations);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETIKGROUPCALCULATION_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------


// --------------------------------------------------------------------------------------
// simIK.addIkElement
// --------------------------------------------------------------------------------------
#define LUA_ADDIKELEMENT_COMMAND_PLUGIN "simIK.addIkElement@IK"
#define LUA_ADDIKELEMENT_COMMAND "simIK.addIkElement"

const int inArgs_ADDIKELEMENT[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_ADDIKELEMENT_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int elementHandle=-1;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_ADDIKELEMENT,inArgs_ADDIKELEMENT[0],LUA_ADDIKELEMENT_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        int tipDummyHandle=inData->at(2).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikAddIkElement(ikGroupHandle,tipDummyHandle,&elementHandle);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_ADDIKELEMENT_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(elementHandle));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getIkElementFlags
// --------------------------------------------------------------------------------------
#define LUA_GETIKELEMENTFLAGS_COMMAND_PLUGIN "simIK.getIkElementFlags@IK"
#define LUA_GETIKELEMENTFLAGS_COMMAND "simIK.getIkElementFlags"

const int inArgs_GETIKELEMENTFLAGS[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETIKELEMENTFLAGS_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int flags=0;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETIKELEMENTFLAGS,inArgs_GETIKELEMENTFLAGS[0],LUA_GETIKELEMENTFLAGS_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        int ikElementHandle=inData->at(2).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetIkElementFlags(ikGroupHandle,ikElementHandle,&flags);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETIKELEMENTFLAGS_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(flags));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setIkElementFlags
// --------------------------------------------------------------------------------------
#define LUA_SETIKELEMENTFLAGS_COMMAND_PLUGIN "simIK.setIkElementFlags@IK"
#define LUA_SETIKELEMENTFLAGS_COMMAND "simIK.setIkElementFlags"

const int inArgs_SETIKELEMENTFLAGS[]={
    4,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_SETIKELEMENTFLAGS_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETIKELEMENTFLAGS,inArgs_SETIKELEMENTFLAGS[0],LUA_SETIKELEMENTFLAGS_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        int ikElementHandle=inData->at(2).int32Data[0];
        int flags=inData->at(3).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetIkElementFlags(ikGroupHandle,ikElementHandle,flags);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETIKELEMENTFLAGS_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getIkElementBase
// --------------------------------------------------------------------------------------
#define LUA_GETIKELEMENTBASE_COMMAND_PLUGIN "simIK.getIkElementBase@IK"
#define LUA_GETIKELEMENTBASE_COMMAND "simIK.getIkElementBase"

const int inArgs_GETIKELEMENTBASE[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETIKELEMENTBASE_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int baseHandle=-1;
    int constrBaseHandle=-1;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETIKELEMENTBASE,inArgs_GETIKELEMENTBASE[0],LUA_GETIKELEMENTBASE_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        int ikElementHandle=inData->at(2).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetIkElementBase(ikGroupHandle,ikElementHandle,&baseHandle,&constrBaseHandle);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETIKELEMENTBASE_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(baseHandle));
        D.pushOutData(CScriptFunctionDataItem(constrBaseHandle));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setIkElementBase
// --------------------------------------------------------------------------------------
#define LUA_SETIKELEMENTBASE_COMMAND_PLUGIN "simIK.setIkElementBase@IK"
#define LUA_SETIKELEMENTBASE_COMMAND "simIK.setIkElementBase"

const int inArgs_SETIKELEMENTBASE[]={
    5,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32|SIM_SCRIPT_ARG_NULL_ALLOWED,0,
};

void LUA_SETIKELEMENTBASE_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETIKELEMENTBASE,inArgs_SETIKELEMENTBASE[0]-1,LUA_SETIKELEMENTBASE_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        int ikElementHandle=inData->at(2).int32Data[0];
        int baseHandle=inData->at(3).int32Data[0];
        int constrBaseHandle=-1;
        if ( (inData->size()>4)&&(inData->at(4).int32Data.size()==1) )
            constrBaseHandle=inData->at(4).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetIkElementBase(ikGroupHandle,ikElementHandle,baseHandle,constrBaseHandle);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETIKELEMENTBASE_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getIkElementConstraints
// --------------------------------------------------------------------------------------
#define LUA_GETIKELEMENTCONSTRAINTS_COMMAND_PLUGIN "simIK.getIkElementConstraints@IK"
#define LUA_GETIKELEMENTCONSTRAINTS_COMMAND "simIK.getIkElementConstraints"

const int inArgs_GETIKELEMENTCONSTRAINTS[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETIKELEMENTCONSTRAINTS_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int constraints=0;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETIKELEMENTCONSTRAINTS,inArgs_GETIKELEMENTCONSTRAINTS[0],LUA_GETIKELEMENTCONSTRAINTS_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        int ikElementHandle=inData->at(2).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetIkElementConstraints(ikGroupHandle,ikElementHandle,&constraints);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETIKELEMENTCONSTRAINTS_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(constraints));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setIkElementConstraints
// --------------------------------------------------------------------------------------
#define LUA_SETIKELEMENTCONSTRAINTS_COMMAND_PLUGIN "simIK.setIkElementConstraints@IK"
#define LUA_SETIKELEMENTCONSTRAINTS_COMMAND "simIK.setIkElementConstraints"

const int inArgs_SETIKELEMENTCONSTRAINTS[]={
    4,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_SETIKELEMENTCONSTRAINTS_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETIKELEMENTCONSTRAINTS,inArgs_SETIKELEMENTCONSTRAINTS[0],LUA_SETIKELEMENTCONSTRAINTS_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        int ikElementHandle=inData->at(2).int32Data[0];
        int constraints=inData->at(3).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetIkElementConstraints(ikGroupHandle,ikElementHandle,constraints);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETIKELEMENTCONSTRAINTS_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getIkElementPrecision
// --------------------------------------------------------------------------------------
#define LUA_GETIKELEMENTPRECISION_COMMAND_PLUGIN "simIK.getIkElementPrecision@IK"
#define LUA_GETIKELEMENTPRECISION_COMMAND "simIK.getIkElementPrecision"

const int inArgs_GETIKELEMENTPRECISION[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETIKELEMENTPRECISION_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    double precision[2];
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETIKELEMENTPRECISION,inArgs_GETIKELEMENTPRECISION[0],LUA_GETIKELEMENTPRECISION_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        int ikElementHandle=inData->at(2).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetIkElementPrecision(ikGroupHandle,ikElementHandle,precision+0,precision+1);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETIKELEMENTPRECISION_COMMAND,err.c_str());
    }
    if (result)
    {
        std::vector<double> v(precision,precision+2);
        D.pushOutData(CScriptFunctionDataItem(v));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setIkElementPrecision
// --------------------------------------------------------------------------------------
#define LUA_SETIKELEMENTPRECISION_COMMAND_PLUGIN "simIK.setIkElementPrecision@IK"
#define LUA_SETIKELEMENTPRECISION_COMMAND "simIK.setIkElementPrecision"

const int inArgs_SETIKELEMENTPRECISION[]={
    4,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_double|sim_script_arg_table,2,
};

void LUA_SETIKELEMENTPRECISION_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETIKELEMENTPRECISION,inArgs_SETIKELEMENTPRECISION[0],LUA_SETIKELEMENTPRECISION_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        int ikElementHandle=inData->at(2).int32Data[0];
        double* precision=&inData->at(3).doubleData[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetIkElementPrecision(ikGroupHandle,ikElementHandle,precision[0],precision[1]);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETIKELEMENTPRECISION_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getIkElementWeights
// --------------------------------------------------------------------------------------
#define LUA_GETIKELEMENTWEIGHTS_COMMAND_PLUGIN "simIK.getIkElementWeights@IK"
#define LUA_GETIKELEMENTWEIGHTS_COMMAND "simIK.getIkElementWeights"

const int inArgs_GETIKELEMENTWEIGHTS[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETIKELEMENTWEIGHTS_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    double weights[2];
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETIKELEMENTWEIGHTS,inArgs_GETIKELEMENTWEIGHTS[0],LUA_GETIKELEMENTWEIGHTS_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        int ikElementHandle=inData->at(2).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                result=ikGetIkElementWeights(ikGroupHandle,ikElementHandle,weights+0,weights+1);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETIKELEMENTWEIGHTS_COMMAND,err.c_str());
    }
    if (result)
    {
        std::vector<double> v(weights,weights+2);
        D.pushOutData(CScriptFunctionDataItem(v));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setIkElementWeights
// --------------------------------------------------------------------------------------
#define LUA_SETIKELEMENTWEIGHTS_COMMAND_PLUGIN "simIK.setIkElementWeights@IK"
#define LUA_SETIKELEMENTWEIGHTS_COMMAND "simIK.setIkElementWeights"

const int inArgs_SETIKELEMENTWEIGHTS[]={
    4,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_double|sim_script_arg_table,2,
};

void LUA_SETIKELEMENTWEIGHTS_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETIKELEMENTWEIGHTS,inArgs_SETIKELEMENTWEIGHTS[0],LUA_SETIKELEMENTWEIGHTS_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        int ikElementHandle=inData->at(2).int32Data[0];
        double* weights=&inData->at(3).doubleData[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool result=ikSetIkElementWeights(ikGroupHandle,ikElementHandle,weights[0],weights[1]);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETIKELEMENTWEIGHTS_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
static std::string jacobianCallback_funcName;
static int jacobianCallback_scriptHandle;
static int jacobianCallback_envId;

bool jacobianCallback(const int jacobianSize[2],std::vector<double>* jacobian,const simInt* rowConstraints,const simInt* rowIkElements,const simInt* colHandles,const simInt* colStages,std::vector<double>* errorVector,double* qVector)
{
    unlockInterface(); // actually required to correctly support CoppeliaSim's old GUI-based IK
    bool retVal=false; // if true jointValues were computed
    int stack=simCreateStack();
    int cols=jacobianSize[1];
    simPushInt32TableOntoStack(stack,rowConstraints,jacobianSize[0]);
    simPushInt32TableOntoStack(stack,rowIkElements,jacobianSize[0]);
    simPushInt32TableOntoStack(stack,colHandles,cols);
    simPushInt32TableOntoStack(stack,colStages,cols);
    simPushDoubleTableOntoStack(stack,jacobian->data(),jacobianSize[0]*jacobianSize[1]);
    simPushDoubleTableOntoStack(stack,errorVector->data(),jacobianSize[0]);
    if (simCallScriptFunctionEx(jacobianCallback_scriptHandle,jacobianCallback_funcName.c_str(),stack)!=-1)
    {
        if (simGetStackSize(stack)==1)
        {
            int s=simGetStackTableInfo(stack,0);
            if (s==jacobianSize[1])
            { // we receive the joint values (the solution)
                retVal=true;
                simGetStackDoubleTable(stack,qVector,s);
            }
        }
        if (simGetStackSize(stack)==2)
        { // we received the Jacobian and the error vector
            // First the error vector:
            int rows=simGetStackTableInfo(stack,0);

            if (rows==jacobianSize[0])
            { // we receive the updated error vector
                errorVector->resize(rows*cols);
                simGetStackDoubleTable(stack,errorVector->data(),rows);
                simPopStackItem(stack,1);
                // Now the Jacobian:
                int r=simGetStackTableInfo(stack,0);
                if (r==rows*cols)
                { // we receive the updated Jacobian
                    jacobian->resize(r);
                    simGetStackDoubleTable(stack,jacobian->data(),r);
                }
            }
        }
    }
    simReleaseStack(stack);
    lockInterface(); // actually required to correctly support CoppeliaSim's old GUI-based IK
    ikSwitchEnvironment(jacobianCallback_envId); // actually required to correctly support CoppeliaSim's old GUI-based IK
    return(retVal);
}
// --------------------------------------------------------------------------------------
// simIK.handleIkGroup
// --------------------------------------------------------------------------------------
#define LUA_HANDLEIKGROUP_COMMAND_PLUGIN "simIK._handleIkGroup@IK"
#define LUA_HANDLEIKGROUP_COMMAND "simIK._handleIkGroup"

const int inArgs_HANDLEIKGROUP[]={
    4,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_string|SIM_SCRIPT_ARG_NULL_ALLOWED,0, // cb func name
    sim_script_arg_int32|SIM_SCRIPT_ARG_NULL_ALLOWED,0, // script handle of cb
};

void LUA_HANDLEIKGROUP_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int ikRes=-1;
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_HANDLEIKGROUP,inArgs_HANDLEIKGROUP[0]-3,LUA_HANDLEIKGROUP_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=ik_handle_all;
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                bool(*cb)(const simInt*,std::vector<double>*,const simInt*,const simInt*,const simInt*,const simInt*,std::vector<double>*,double*)=nullptr;
                if ( (inData->size()>1)&&(inData->at(1).int32Data.size()==1) )
                    ikGroupHandle=inData->at(1).int32Data[0];
                if ( (inData->size()>3)&&(inData->at(2).stringData.size()==1)&&(inData->at(2).stringData[0].size()>0)&&(inData->at(3).int32Data.size()==1) )
                {
                    jacobianCallback_funcName=inData->at(2).stringData[0];
                    jacobianCallback_scriptHandle=inData->at(3).int32Data[0];
                    jacobianCallback_envId=envId;
                    cb=jacobianCallback;
                }
                result=ikHandleIkGroup(ikGroupHandle,&ikRes,cb);
                if (!result)
                    err=ikGetLastError();
            }
            else
                err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_HANDLEIKGROUP_COMMAND,err.c_str());
    }
    if (result)
    {
        D.pushOutData(CScriptFunctionDataItem(ikRes));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

static std::string validationCallback_funcNameAtScriptName;
static int validationCallback_scriptType;
static int validationCallback_envId;
static size_t validationCallback_jointCnt;

bool validationCallback(double* conf)
{
    unlockInterface(); // actually required to correctly support CoppeliaSim's old GUI-based IK
    simBool retVal=1;
    int stack=simCreateStack();
    simPushDoubleTableOntoStack(stack,conf,int(validationCallback_jointCnt));
    if (simCallScriptFunctionEx(validationCallback_scriptType,validationCallback_funcNameAtScriptName.c_str(),stack)!=-1)
        simGetStackBoolValue(stack,&retVal);
    simReleaseStack(stack);
    lockInterface(); // actually required to correctly support CoppeliaSim's old GUI-based IK
    ikSwitchEnvironment(validationCallback_envId); // actually required to correctly support CoppeliaSim's old GUI-based IK
    return(retVal!=0);
}

// --------------------------------------------------------------------------------------
// simIK.getConfigForTipPose // deprecated
// --------------------------------------------------------------------------------------
#define LUA_GETCONFIGFORTIPPOSE_COMMAND_PLUGIN "simIK._getConfigForTipPose@IK"
#define LUA_GETCONFIGFORTIPPOSE_COMMAND "simIK._getConfigForTipPose"

const int inArgs_GETCONFIGFORTIPPOSE[]={
    11,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32|sim_script_arg_table,0,
    sim_script_arg_double|SIM_SCRIPT_ARG_NULL_ALLOWED,0,
    sim_script_arg_int32|SIM_SCRIPT_ARG_NULL_ALLOWED,0,
    sim_script_arg_double|sim_script_arg_table|SIM_SCRIPT_ARG_NULL_ALLOWED,4,
    sim_script_arg_string|SIM_SCRIPT_ARG_NULL_ALLOWED,0,
    sim_script_arg_int32|SIM_SCRIPT_ARG_NULL_ALLOWED,0,
    sim_script_arg_int32|sim_script_arg_table|SIM_SCRIPT_ARG_NULL_ALLOWED,0,
    sim_script_arg_double|sim_script_arg_table|SIM_SCRIPT_ARG_NULL_ALLOWED,0,
    sim_script_arg_double|sim_script_arg_table|SIM_SCRIPT_ARG_NULL_ALLOWED,0,
};

void LUA_GETCONFIGFORTIPPOSE_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int calcResult=-1;
    double* retConfig=nullptr;
    size_t jointCnt=0;
    if (D.readDataFromStack(p->stackID,inArgs_GETCONFIGFORTIPPOSE,inArgs_GETCONFIGFORTIPPOSE[0]-8,LUA_GETCONFIGFORTIPPOSE_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                jointCnt=inData->at(2).int32Data.size();
                if (jointCnt>0)
                {
                    retConfig=new double[jointCnt];
                    double thresholdDist=double(0.1);
                    int iterations=1000;
                    double* metric=nullptr;
                    int* jointOptions=nullptr;
                    double* lowLimits=nullptr;
                    double* ranges=nullptr;
                    bool(*cb)(double*)=nullptr;
                    int scriptType=sim_scripttype_childscript;
                    if ( (inData->size()>4)&&(inData->at(4).int32Data.size()==1) )
                        iterations=inData->at(4).int32Data[0];
                    if ( (inData->size()>8)&&(inData->at(8).int32Data.size()>=jointCnt) )
                        jointOptions=&inData->at(8).int32Data[0];
                    if ( (inData->size()>7)&&(inData->at(7).int32Data.size()==1) )
                        scriptType=inData->at(7).int32Data[0];
                    if ( (inData->size()>6)&&(inData->at(6).stringData.size()==1)&&(inData->at(6).stringData[0].size()>0) )
                    {
                        validationCallback_funcNameAtScriptName=inData->at(6).stringData[0];
                        validationCallback_scriptType=scriptType;
                        validationCallback_jointCnt=jointCnt;
                        validationCallback_envId=envId;
                        cb=validationCallback;
                    }
                    if ( (inData->size()>3)&&(inData->at(3).doubleData.size()==1) )
                        thresholdDist=inData->at(3).doubleData[0];
                    if ( (inData->size()>5)&&(inData->at(5).doubleData.size()>=4) )
                        metric=&inData->at(5).doubleData[0];
                    if ( (inData->size()>9)&&(inData->at(9).doubleData.size()>=jointCnt) )
                        lowLimits=&inData->at(9).doubleData[0];
                    if ( (inData->size()>10)&&(inData->at(10).doubleData.size()>=jointCnt) )
                        ranges=&inData->at(10).doubleData[0];
                    calcResult=ikGetConfigForTipPose(ikGroupHandle,jointCnt,&inData->at(2).int32Data[0],thresholdDist,iterations,retConfig,metric,cb,jointOptions,lowLimits,ranges);
                    if (calcResult==-1)
                         err=ikGetLastError();
                }
                else
                    err="invalid joint handles";
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETCONFIGFORTIPPOSE_COMMAND,err.c_str());
    }
    if (calcResult==1)
    {
        std::vector<double> v(retConfig,retConfig+jointCnt);
        D.pushOutData(CScriptFunctionDataItem(v));
        D.writeDataToStack(p->stackID);
    }
    if (retConfig!=nullptr)
        delete[] retConfig;
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.findConfig
// --------------------------------------------------------------------------------------
#define LUA_FINDCONFIG_COMMAND_PLUGIN "simIK._findConfig@IK"
#define LUA_FINDCONFIG_COMMAND "simIK._findConfig"

const int inArgs_FINDCONFIG[]={
    8,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32|sim_script_arg_table,0,
    sim_script_arg_double|SIM_SCRIPT_ARG_NULL_ALLOWED,0,
    sim_script_arg_int32|SIM_SCRIPT_ARG_NULL_ALLOWED,0,
    sim_script_arg_double|sim_script_arg_table|SIM_SCRIPT_ARG_NULL_ALLOWED,4,
    sim_script_arg_string|SIM_SCRIPT_ARG_NULL_ALLOWED,0, // cb func name
    sim_script_arg_int32|SIM_SCRIPT_ARG_NULL_ALLOWED,0, // script handle of cb
};

void LUA_FINDCONFIG_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    int calcResult=-1;
    double* retConfig=nullptr;
    size_t jointCnt=0;
    if (D.readDataFromStack(p->stackID,inArgs_FINDCONFIG,inArgs_FINDCONFIG[0]-5,LUA_FINDCONFIG_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                jointCnt=inData->at(2).int32Data.size();
                if (jointCnt>0)
                {
                    retConfig=new double[jointCnt];
                    double thresholdDist=double(0.1);
                    int timeInMs=100;
                    double* metric=nullptr;
                    bool(*cb)(double*)=nullptr;
                    int scriptType=sim_scripttype_childscript;
                    if ( (inData->size()>4)&&(inData->at(4).int32Data.size()==1) )
                        timeInMs=inData->at(4).int32Data[0];
                    if ( (inData->size()>7)&&(inData->at(7).int32Data.size()==1) )
                        scriptType=inData->at(7).int32Data[0];
                    if ( (inData->size()>6)&&(inData->at(6).stringData.size()==1)&&(inData->at(6).stringData[0].size()>0) )
                    {
                        validationCallback_funcNameAtScriptName=inData->at(6).stringData[0];
                        validationCallback_scriptType=scriptType;
                        validationCallback_jointCnt=jointCnt;
                        validationCallback_envId=envId;
                        cb=validationCallback;
                    }
                    if ( (inData->size()>3)&&(inData->at(3).doubleData.size()==1) )
                        thresholdDist=inData->at(3).doubleData[0];
                    if ( (inData->size()>5)&&(inData->at(5).doubleData.size()>=4) )
                        metric=&inData->at(5).doubleData[0];
                    calcResult=ikFindConfig(ikGroupHandle,jointCnt,&inData->at(2).int32Data[0],thresholdDist,timeInMs,retConfig,metric,cb);
                    if (calcResult==-1)
                         err=ikGetLastError();
                }
                else
                    err="invalid joint handles";
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_FINDCONFIG_COMMAND,err.c_str());
    }
    if (calcResult==1)
    {
        std::vector<double> v(retConfig,retConfig+jointCnt);
        D.pushOutData(CScriptFunctionDataItem(v));
        D.writeDataToStack(p->stackID);
    }
    if (retConfig!=nullptr)
        delete[] retConfig;
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getObjectTransformation
// --------------------------------------------------------------------------------------
#define LUA_GETOBJECTTRANSFORMATION_COMMAND_PLUGIN "simIK.getObjectTransformation@IK"
#define LUA_GETOBJECTTRANSFORMATION_COMMAND "simIK.getObjectTransformation"

const int inArgs_GETOBJECTTRANSFORMATION[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETOBJECTTRANSFORMATION_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    double pos[3];
    double q[4];
    double e[3];
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETOBJECTTRANSFORMATION,inArgs_GETOBJECTTRANSFORMATION[0],LUA_GETOBJECTTRANSFORMATION_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int objHandle=inData->at(1).int32Data[0];
        int relHandle=inData->at(2).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                C7Vector tr;
                result=ikGetObjectTransformation(objHandle,relHandle,&tr);
                if (result)
                {   // CoppeliaSim quaternion, internally: w x y z
                    // CoppeliaSim quaternion, at interfaces: x y z w
                    tr.X.getData(pos);
                    q[0]=tr.Q(1);
                    q[1]=tr.Q(2);
                    q[2]=tr.Q(3);
                    q[3]=tr.Q(0);
                    tr.Q.getEulerAngles().getData(e);
                }
                else
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETOBJECTTRANSFORMATION_COMMAND,err.c_str());
    }
    if (result)
    {
        std::vector<double> _pos(pos,pos+3);
        D.pushOutData(CScriptFunctionDataItem(_pos));
        std::vector<double> _q(q,q+4);
        D.pushOutData(CScriptFunctionDataItem(_q));
        std::vector<double> _e(e,e+3);
        D.pushOutData(CScriptFunctionDataItem(_e));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setObjectTransformation
// --------------------------------------------------------------------------------------
#define LUA_SETOBJECTTRANSFORMATION_COMMAND_PLUGIN "simIK.setObjectTransformation@IK"
#define LUA_SETOBJECTTRANSFORMATION_COMMAND "simIK.setObjectTransformation"

const int inArgs_SETOBJECTTRANSFORMATION[]={
    5,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_double|sim_script_arg_table,3,
    sim_script_arg_double|sim_script_arg_table,3,
};

void LUA_SETOBJECTTRANSFORMATION_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETOBJECTTRANSFORMATION,inArgs_SETOBJECTTRANSFORMATION[0],LUA_SETOBJECTTRANSFORMATION_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int objHandle=inData->at(1).int32Data[0];
        int relHandle=inData->at(2).int32Data[0];
        double* quat=nullptr;
        double* euler=nullptr;
        double* pos=&inData->at(3).doubleData[0];
        if (inData->at(4).doubleData.size()==3)
            euler=&inData->at(4).doubleData[0];
        else
            quat=&inData->at(4).doubleData[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {   // CoppeliaSim quaternion, internally: w x y z
                // CoppeliaSim quaternion, at interfaces: x y z w
                C7Vector tr;
                tr.X=C3Vector(pos);
                if (euler!=nullptr)
                    tr.Q.setEulerAngles(C3Vector(euler));
                if (quat!=nullptr)
                    tr.Q=C4Vector(quat[3],quat[0],quat[1],quat[2]);
                bool result=ikSetObjectTransformation(objHandle,relHandle,&tr);
                if (!result)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETOBJECTTRANSFORMATION_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getObjectMatrix
// --------------------------------------------------------------------------------------
#define LUA_GETOBJECTMATRIX_COMMAND_PLUGIN "simIK.getObjectMatrix@IK"
#define LUA_GETOBJECTMATRIX_COMMAND "simIK.getObjectMatrix"

const int inArgs_GETOBJECTMATRIX[]={
    3,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETOBJECTMATRIX_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    double matr[12];
    bool result=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETOBJECTMATRIX,inArgs_GETOBJECTMATRIX[0],LUA_GETOBJECTMATRIX_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int objHandle=inData->at(1).int32Data[0];
        int relHandle=inData->at(2).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                C7Vector tr;
                result=ikGetObjectTransformation(objHandle,relHandle,&tr);
                if (result)
                {
                    C4X4Matrix m(tr.getMatrix());
                    m.getData(matr);
                }
                else
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETOBJECTMATRIX_COMMAND,err.c_str());
    }
    if (result)
    {
        std::vector<double> _matr(matr,matr+12);
        D.pushOutData(CScriptFunctionDataItem(_matr));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.setObjectMatrix
// --------------------------------------------------------------------------------------
#define LUA_SETOBJECTMATRIX_COMMAND_PLUGIN "simIK.setObjectMatrix@IK"
#define LUA_SETOBJECTMATRIX_COMMAND "simIK.setObjectMatrix"

const int inArgs_SETOBJECTMATRIX[]={
    4,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
    sim_script_arg_double|sim_script_arg_table,12,
};

void LUA_SETOBJECTMATRIX_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_SETOBJECTMATRIX,inArgs_SETOBJECTMATRIX[0],LUA_SETOBJECTMATRIX_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int objHandle=inData->at(1).int32Data[0];
        int relHandle=inData->at(2).int32Data[0];
        double* m=&inData->at(3).doubleData[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                C4X4Matrix _m;
                _m.setData(m);
                C7Vector tr(_m.getTransformation());
                bool result=ikSetObjectTransformation(objHandle,relHandle,&tr);
                if (!result)
                    err=ikGetLastError();
            }
            else
                err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_SETOBJECTMATRIX_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.computeJacobian
// --------------------------------------------------------------------------------------
#define LUA_COMPUTEJACOBIAN_COMMAND_PLUGIN "simIK.computeJacobian@IK"
#define LUA_COMPUTEJACOBIAN_COMMAND "simIK.computeJacobian"

const int inArgs_COMPUTEJACOBIAN[]={
    7,
    sim_script_arg_int32,0, // Ik env
    sim_script_arg_int32,0, // base handle (prev. ik group handle)
    sim_script_arg_int32,0, // last joint handle (prev. options)
    sim_script_arg_int32,0, // constraints
    sim_script_arg_double|sim_script_arg_table,12, // tip pose
    sim_script_arg_double|sim_script_arg_table,12, // target pose, optional
    sim_script_arg_int32,0, // alt base handle, optional
};

void LUA_COMPUTEJACOBIAN_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    if (D.readDataFromStack(p->stackID,inArgs_COMPUTEJACOBIAN,inArgs_COMPUTEJACOBIAN[0]-4,LUA_COMPUTEJACOBIAN_COMMAND))
    {
        std::string err;
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        if (inData->size()>=4)
        {
            int baseHandle=inData->at(1).int32Data[0];
            int jointHandle=inData->at(2).int32Data[0];
            int altBaseHandle=-1;
            if ( (inData->size()>=5)&&(inData->at(3).int32Data.size()==1)&&(inData->at(4).doubleData.size()>=7) )
            {
                int constraints=inData->at(3).int32Data[0];
                C7Vector tipPose;
                if (inData->at(4).doubleData.size()<12)
                    tipPose.setData(inData->at(4).doubleData.data(),true);
                else
                {
                    C4X4Matrix m;
                    m.setData(inData->at(4).doubleData.data());
                    tipPose=m.getTransformation();
                }
                C7Vector targetPose(tipPose);
                bool ok=true;
                if (inData->size()>=6)
                {
                    if (inData->at(5).doubleData.size()>=7)
                    {
                        if (inData->at(5).doubleData.size()<12)
                            targetPose.setData(inData->at(5).doubleData.data(),true);
                        else
                        {
                            C4X4Matrix m;
                            m.setData(inData->at(5).doubleData.data());
                            targetPose=m.getTransformation();
                        }
                        if (inData->size()>=7)
                        {
                            if (inData->at(3).int32Data.size()==1)
                                altBaseHandle=inData->at(3).int32Data[0];
                            else
                                ok=false;
                        }
                    }
                    else
                        ok=false;
                }
                if (ok)
                {
                    std::vector<double> jacobian;
                    std::vector<double> errorVect;
                    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
                    if (ikSwitchEnvironment(envId))
                    {
                        if (ikComputeJacobian(baseHandle,altBaseHandle,jointHandle,constraints,&tipPose,&targetPose,&jacobian,&errorVect))
                        {
                            D.pushOutData(CScriptFunctionDataItem(jacobian));
                            D.pushOutData(CScriptFunctionDataItem(errorVect));
                            D.writeDataToStack(p->stackID);
                        }
                        else
                            err=ikGetLastError();
                    }
                    else
                        err=ikGetLastError();
                }
                else
                    err="invalid arguments";
            }
            else
                err="invalid arguments";
        }
        else
        { // old, for backw. compatibility
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                int ikGroupHandle=inData->at(1).int32Data[0];
                int options=inData->at(2).int32Data[0];
                bool retVal=false;
                bool success=false;
                success=ikComputeJacobian_old(ikGroupHandle,options,&retVal); // old back compatibility function
                if (success)
                {
                    D.pushOutData(CScriptFunctionDataItem(retVal));
                    D.writeDataToStack(p->stackID);
                }
                else
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_COMPUTEJACOBIAN_COMMAND,err.c_str());
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getJacobian, deprecated on 25.10.2022
// --------------------------------------------------------------------------------------
#define LUA_GETJACOBIAN_COMMAND_PLUGIN "simIK.getJacobian@IK"
#define LUA_GETJACOBIAN_COMMAND "simIK.getJacobian"

const int inArgs_GETJACOBIAN[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETJACOBIAN_CALLBACK(SScriptCallBack* p)
{ // deprecated on 25.10.2022
    CScriptFunctionData D;
    double* matr=nullptr;
    size_t matrSize[2];
    if (D.readDataFromStack(p->stackID,inArgs_GETJACOBIAN,inArgs_GETJACOBIAN[0],LUA_GETJACOBIAN_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
                matr=ikGetJacobian_old(ikGroupHandle,matrSize);
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETJACOBIAN_COMMAND,err.c_str());
    }
    if (matr!=nullptr)
    {
        std::vector<double> v1(matr,matr+matrSize[0]*matrSize[1]);
        D.pushOutData(CScriptFunctionDataItem(v1));
        std::vector<int> v2;
        v2.push_back(int(matrSize[0]));
        v2.push_back(int(matrSize[1]));
        D.pushOutData(CScriptFunctionDataItem(v2));
        D.writeDataToStack(p->stackID);
        ikReleaseBuffer(matr);
    }
}
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// simIK.getManipulability
// --------------------------------------------------------------------------------------
#define LUA_GETMANIPULABILITY_COMMAND_PLUGIN "simIK.getManipulability@IK"
#define LUA_GETMANIPULABILITY_COMMAND "simIK.getManipulability"

const int inArgs_GETMANIPULABILITY[]={
    2,
    sim_script_arg_int32,0,
    sim_script_arg_int32,0,
};

void LUA_GETMANIPULABILITY_CALLBACK(SScriptCallBack* p)
{
    CScriptFunctionData D;
    double retVal=0.0;
    bool success=false;
    if (D.readDataFromStack(p->stackID,inArgs_GETMANIPULABILITY,inArgs_GETMANIPULABILITY[0],LUA_GETMANIPULABILITY_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
        int envId=inData->at(0).int32Data[0];
        int ikGroupHandle=inData->at(1).int32Data[0];
        std::string err;
        {
            CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
            if (ikSwitchEnvironment(envId))
            {
                success=ikGetManipulability_old(ikGroupHandle,&retVal);
                if (!success)
                     err=ikGetLastError();
            }
            else
                 err=ikGetLastError();
        }
        if (err.size()>0)
            simSetLastError(LUA_GETMANIPULABILITY_COMMAND,err.c_str());
    }
    if (success)
    {
        D.pushOutData(CScriptFunctionDataItem(retVal));
        D.writeDataToStack(p->stackID);
    }
}
// --------------------------------------------------------------------------------------


SIM_DLLEXPORT unsigned char simStart(void*,int)
{
    char curDirAndFile[1024];
#ifdef _WIN32
    #ifdef QT_COMPIL
        _getcwd(curDirAndFile, sizeof(curDirAndFile));
    #else
        GetModuleFileName(NULL,curDirAndFile,1023);
        PathRemoveFileSpec(curDirAndFile);
    #endif
#elif defined (__linux) || defined (__APPLE__)
    getcwd(curDirAndFile, sizeof(curDirAndFile));
#endif

    std::string currentDirAndPath(curDirAndFile);
    std::string temp(currentDirAndPath);

#ifdef _WIN32
    temp+="\\coppeliaSim.dll";
#elif defined (__linux)
    temp+="/libcoppeliaSim.so";
#elif defined (__APPLE__)
    temp+="/libcoppeliaSim.dylib";
#endif /* __linux || __APPLE__ */

    simLib=loadSimLibrary(temp.c_str());
    if (simLib==NULL)
    {
        printf("simExtIK: error: could not find or correctly load the CoppeliaSim library. Cannot start the plugin.\n"); // cannot use simAddLog here.
        return(0);
    }
    if (getSimProcAddresses(simLib)==0)
    {
        printf("simExtIK: error: could not find all required functions in the CoppeliaSim library. Cannot start the plugin.\n"); // cannot use simAddLog here.
        unloadSimLibrary(simLib);
        return(0);
    }
    
    simRegisterScriptVariable("simIK","require('simIK')",0);

    // Register the new Lua commands:
    simRegisterScriptCallbackFunction(LUA_CREATEENVIRONMENT_COMMAND_PLUGIN,strConCat("int environmentHandle=",LUA_CREATEENVIRONMENT_COMMAND,"()"),LUA_CREATEENVIRONMENT_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_ERASEENVIRONMENT_COMMAND_PLUGIN,nullptr,LUA_ERASEENVIRONMENT_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_DUPLICATEENVIRONMENT_COMMAND_PLUGIN,strConCat("int duplicateEnvHandle=",LUA_DUPLICATEENVIRONMENT_COMMAND,"(int environmentHandle)"),LUA_DUPLICATEENVIRONMENT_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_LOAD_COMMAND_PLUGIN,strConCat("",LUA_LOAD_COMMAND,"(int environmentHandle,string data)"),LUA_LOAD_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SAVE_COMMAND_PLUGIN,strConCat("string data=",LUA_SAVE_COMMAND,"(int environmentHandle)"),LUA_SAVE_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETOBJECTS_COMMAND_PLUGIN,strConCat("int objectHandle,string objectName,bool isJoint,int jointType=",LUA_GETOBJECTS_COMMAND,"(int environmentHandle,int index)"),LUA_GETOBJECTS_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETOBJECTHANDLE_COMMAND_PLUGIN,strConCat("int objectHandle=",LUA_GETOBJECTHANDLE_COMMAND,"(int environmentHandle,string objectName)"),LUA_GETOBJECTHANDLE_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_DOESOBJECTEXIST_COMMAND_PLUGIN,strConCat("bool result=",LUA_DOESOBJECTEXIST_COMMAND,"(int environmentHandle,string objectName)"),LUA_DOESOBJECTEXIST_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_ERASEOBJECT_COMMAND_PLUGIN,strConCat("",LUA_ERASEOBJECT_COMMAND,"(int environmentHandle,int objectHandle)"),LUA_ERASEOBJECT_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETOBJECTPARENT_COMMAND_PLUGIN,strConCat("int parentObjectHandle=",LUA_GETOBJECTPARENT_COMMAND,"(int environmentHandle,int objectHandle)"),LUA_GETOBJECTPARENT_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETOBJECTPARENT_COMMAND_PLUGIN,strConCat("",LUA_SETOBJECTPARENT_COMMAND,"(int environmentHandle,int objectHandle,int parentObjectHandle, bool keepInPlace=true)"),LUA_SETOBJECTPARENT_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_CREATEDUMMY_COMMAND_PLUGIN,strConCat("int dummyHandle=",LUA_CREATEDUMMY_COMMAND,"(int environmentHandle,string dummyName='')"),LUA_CREATEDUMMY_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETLINKEDDUMMY_COMMAND_PLUGIN,strConCat("int linkedDummyHandle=",LUA_GETLINKEDDUMMY_COMMAND,"(int environmentHandle,int dummyHandle)"),LUA_GETLINKEDDUMMY_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETLINKEDDUMMY_COMMAND_PLUGIN,strConCat("",LUA_SETLINKEDDUMMY_COMMAND,"(int environmentHandle,int dummyHandle,int linkedDummyHandle)"),LUA_SETLINKEDDUMMY_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_CREATEJOINT_COMMAND_PLUGIN,strConCat("int jointHandle=",LUA_CREATEJOINT_COMMAND,"(int environmentHandle,int jointType,string jointName='')"),LUA_CREATEJOINT_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETJOINTTYPE_COMMAND_PLUGIN,strConCat("int jointType=",LUA_GETJOINTTYPE_COMMAND,"(int environmentHandle,int jointHandle)"),LUA_GETJOINTTYPE_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETJOINTMODE_COMMAND_PLUGIN,strConCat("int jointMode=",LUA_GETJOINTMODE_COMMAND,"(int environmentHandle,int jointHandle)"),LUA_GETJOINTMODE_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETJOINTMODE_COMMAND_PLUGIN,strConCat("",LUA_SETJOINTMODE_COMMAND,"(int environmentHandle,int jointHandle,int jointMode)"),LUA_SETJOINTMODE_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETJOINTINTERVAL_COMMAND_PLUGIN,strConCat("bool cyclic,float[2] interval=",LUA_GETJOINTINTERVAL_COMMAND,"(int environmentHandle,int jointHandle)"),LUA_GETJOINTINTERVAL_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETJOINTINTERVAL_COMMAND_PLUGIN,strConCat("",LUA_SETJOINTINTERVAL_COMMAND,"(int environmentHandle,int jointHandle,bool cyclic,float[2] interval={})"),LUA_SETJOINTINTERVAL_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETJOINTSCREWPITCH_COMMAND_PLUGIN,strConCat("float pitch=",LUA_GETJOINTSCREWPITCH_COMMAND,"(int environmentHandle,int jointHandle)"),LUA_GETJOINTSCREWPITCH_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETJOINTSCREWPITCH_COMMAND_PLUGIN,strConCat("",LUA_SETJOINTSCREWPITCH_COMMAND,"(int environmentHandle,int jointHandle,float pitch)"),LUA_SETJOINTSCREWPITCH_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETJOINTIKWEIGHT_COMMAND_PLUGIN,strConCat("float weight=",LUA_GETJOINTIKWEIGHT_COMMAND,"(int environmentHandle,int jointHandle)"),LUA_GETJOINTIKWEIGHT_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETJOINTIKWEIGHT_COMMAND_PLUGIN,strConCat("",LUA_SETJOINTIKWEIGHT_COMMAND,"(int environmentHandle,int jointHandle,float weight)"),LUA_SETJOINTIKWEIGHT_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETJOINTLIMITMARGIN_COMMAND_PLUGIN,strConCat("float margin=",LUA_GETJOINTLIMITMARGIN_COMMAND,"(int environmentHandle,int jointHandle)"),LUA_GETJOINTLIMITMARGIN_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETJOINTLIMITMARGIN_COMMAND_PLUGIN,strConCat("",LUA_SETJOINTLIMITMARGIN_COMMAND,"(int environmentHandle,int jointHandle,float margin)"),LUA_SETJOINTLIMITMARGIN_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETJOINTMAXSTEPSIZE_COMMAND_PLUGIN,strConCat("float stepSize=",LUA_GETJOINTMAXSTEPSIZE_COMMAND,"(int environmentHandle,int jointHandle)"),LUA_GETJOINTMAXSTEPSIZE_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETJOINTMAXSTEPSIZE_COMMAND_PLUGIN,strConCat("",LUA_SETJOINTMAXSTEPSIZE_COMMAND,"(int environmentHandle,int jointHandle,float stepSize)"),LUA_SETJOINTMAXSTEPSIZE_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETJOINTDEPENDENCY_COMMAND_PLUGIN,strConCat("int depJointHandle,float offset,float mult=",LUA_GETJOINTDEPENDENCY_COMMAND,"(int environmentHandle,int jointHandle)"),LUA_GETJOINTDEPENDENCY_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETJOINTDEPENDENCY_COMMAND_PLUGIN,strConCat("",LUA_SETJOINTDEPENDENCY_COMMAND,"(int environmentHandle,int jointHandle,int depJointHandle,float offset=0,float mult=1)"),LUA_SETJOINTDEPENDENCY_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETJOINTPOSITION_COMMAND_PLUGIN,strConCat("float position=",LUA_GETJOINTPOSITION_COMMAND,"(int environmentHandle,int jointHandle)"),LUA_GETJOINTPOSITION_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETJOINTPOSITION_COMMAND_PLUGIN,strConCat("",LUA_SETJOINTPOSITION_COMMAND,"(int environmentHandle,int jointHandle,float position)"),LUA_SETJOINTPOSITION_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETJOINTMATRIX_COMMAND_PLUGIN,strConCat("float[12] matrix=",LUA_GETJOINTMATRIX_COMMAND,"(int environmentHandle,int jointHandle)"),LUA_GETJOINTMATRIX_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETSPHERICALJOINTMATRIX_COMMAND_PLUGIN,strConCat("",LUA_SETSPHERICALJOINTMATRIX_COMMAND,"(int environmentHandle,int jointHandle,float[12] matrix)"),LUA_SETSPHERICALJOINTMATRIX_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETJOINTTRANSFORMATION_COMMAND_PLUGIN,strConCat("float[3] position,float[4] quaternion,float[3] euler=",LUA_GETJOINTTRANSFORMATION_COMMAND,"(int environmentHandle,int jointHandle)"),LUA_GETJOINTTRANSFORMATION_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETSPHERICALJOINTROTATION_COMMAND_PLUGIN,strConCat("",LUA_SETSPHERICALJOINTROTATION_COMMAND,"(int environmentHandle,int jointHandle,float[] eulerOrQuaternion)"),LUA_SETSPHERICALJOINTROTATION_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETIKGROUPHANDLE_COMMAND_PLUGIN,strConCat("int ikGroupHandle=",LUA_GETIKGROUPHANDLE_COMMAND,"(int environmentHandle,string ikGroupName)"),LUA_GETIKGROUPHANDLE_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_DOESIKGROUPEXIST_COMMAND_PLUGIN,strConCat("bool result=",LUA_DOESIKGROUPEXIST_COMMAND,"(int environmentHandle,string ikGroupName)"),LUA_DOESIKGROUPEXIST_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_CREATEIKGROUP_COMMAND_PLUGIN,strConCat("int ikGroupHandle=",LUA_CREATEIKGROUP_COMMAND,"(int environmentHandle,string ikGroupName='')"),LUA_CREATEIKGROUP_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETIKGROUPFLAGS_COMMAND_PLUGIN,strConCat("int flags=",LUA_GETIKGROUPFLAGS_COMMAND,"(int environmentHandle,int ikGroupHandle)"),LUA_GETIKGROUPFLAGS_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETIKGROUPFLAGS_COMMAND_PLUGIN,strConCat("",LUA_SETIKGROUPFLAGS_COMMAND,"(int environmentHandle,int ikGroupHandle,int flags)"),LUA_SETIKGROUPFLAGS_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETIKGROUPJOINTLIMITHITS_COMMAND_PLUGIN,strConCat("int[] jointHandles,float[] underOrOvershots=",LUA_GETIKGROUPJOINTLIMITHITS_COMMAND,"(int environmentHandle,int ikGroupHandle)"),LUA_GETIKGROUPJOINTLIMITHITS_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETIKGROUPCALCULATION_COMMAND_PLUGIN,strConCat("int method,float damping,int maxIterations=",LUA_GETIKGROUPCALCULATION_COMMAND,"(int environmentHandle,int ikGroupHandle)"),LUA_GETIKGROUPCALCULATION_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETIKGROUPCALCULATION_COMMAND_PLUGIN,strConCat("",LUA_SETIKGROUPCALCULATION_COMMAND,"(int environmentHandle,int ikGroupHandle,int method,float damping,int maxIterations)"),LUA_SETIKGROUPCALCULATION_CALLBACK);
//    simRegisterScriptCallbackFunction(LUA_GETIKGROUPLIMITTHRESHOLDS_COMMAND_PLUGIN,strConCat("float[2] thresholds=",LUA_GETIKGROUPLIMITTHRESHOLDS_COMMAND,"(int environmentHandle,int ikGroupHandle)"),LUA_GETIKGROUPLIMITTHRESHOLDS_CALLBACK);
//    simRegisterScriptCallbackFunction(LUA_SETIKGROUPLIMITTHRESHOLDS_COMMAND_PLUGIN,strConCat("",LUA_SETIKGROUPLIMITTHRESHOLDS_COMMAND,"(int environmentHandle,int ikGroupHandle,float[2] thresholds)"),LUA_SETIKGROUPLIMITTHRESHOLDS_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_ADDIKELEMENT_COMMAND_PLUGIN,strConCat("int elementHandle=",LUA_ADDIKELEMENT_COMMAND,"(int environmentHandle,int ikGroupHandle,int tipDummyHandle)"),LUA_ADDIKELEMENT_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETIKELEMENTFLAGS_COMMAND_PLUGIN,strConCat("int flags=",LUA_GETIKELEMENTFLAGS_COMMAND,"(int environmentHandle,int ikGroupHandle,int elementHandle)"),LUA_GETIKELEMENTFLAGS_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETIKELEMENTFLAGS_COMMAND_PLUGIN,strConCat("",LUA_SETIKELEMENTFLAGS_COMMAND,"(int environmentHandle,int ikGroupHandle,int elementHandle,int flags)"),LUA_SETIKELEMENTFLAGS_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETIKELEMENTBASE_COMMAND_PLUGIN,strConCat("int baseHandle,int constraintsBaseHandle=",LUA_GETIKELEMENTBASE_COMMAND,"(int environmentHandle,int ikGroupHandle,int elementHandle)"),LUA_GETIKELEMENTBASE_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETIKELEMENTBASE_COMMAND_PLUGIN,strConCat("",LUA_SETIKELEMENTBASE_COMMAND,"(int environmentHandle,int ikGroupHandle,int elementHandle,int baseHandle,int constraintsBaseHandle=-1)"),LUA_SETIKELEMENTBASE_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETIKELEMENTCONSTRAINTS_COMMAND_PLUGIN,strConCat("int constraints=",LUA_GETIKELEMENTCONSTRAINTS_COMMAND,"(int environmentHandle,int ikGroupHandle,int elementHandle)"),LUA_GETIKELEMENTCONSTRAINTS_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETIKELEMENTCONSTRAINTS_COMMAND_PLUGIN,strConCat("",LUA_SETIKELEMENTCONSTRAINTS_COMMAND,"(int environmentHandle,int ikGroupHandle,int elementHandle,int constraints)"),LUA_SETIKELEMENTCONSTRAINTS_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETIKELEMENTPRECISION_COMMAND_PLUGIN,strConCat("float[2] precision=",LUA_GETIKELEMENTPRECISION_COMMAND,"(int environmentHandle,int ikGroupHandle,int elementHandle)"),LUA_GETIKELEMENTPRECISION_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETIKELEMENTPRECISION_COMMAND_PLUGIN,strConCat("",LUA_SETIKELEMENTPRECISION_COMMAND,"(int environmentHandle,int ikGroupHandle,int elementHandle,float[2] precision)"),LUA_SETIKELEMENTPRECISION_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETIKELEMENTWEIGHTS_COMMAND_PLUGIN,strConCat("float[2] weights=",LUA_GETIKELEMENTWEIGHTS_COMMAND,"(int environmentHandle,int ikGroupHandle,int elementHandle)"),LUA_GETIKELEMENTWEIGHTS_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETIKELEMENTWEIGHTS_COMMAND_PLUGIN,strConCat("",LUA_SETIKELEMENTWEIGHTS_COMMAND,"(int environmentHandle,int ikGroupHandle,int elementHandle,float[2] weights)"),LUA_SETIKELEMENTWEIGHTS_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_HANDLEIKGROUP_COMMAND_PLUGIN,nullptr,LUA_HANDLEIKGROUP_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETCONFIGFORTIPPOSE_COMMAND_PLUGIN,nullptr,LUA_GETCONFIGFORTIPPOSE_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_FINDCONFIG_COMMAND_PLUGIN,nullptr,LUA_FINDCONFIG_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETOBJECTTRANSFORMATION_COMMAND_PLUGIN,strConCat("float[3] position,float[4] quaternion,float[3] euler=",LUA_GETOBJECTTRANSFORMATION_COMMAND,"(int environmentHandle,int objectHandle,int relativeToObjectHandle)"),LUA_GETOBJECTTRANSFORMATION_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETOBJECTTRANSFORMATION_COMMAND_PLUGIN,strConCat("",LUA_SETOBJECTTRANSFORMATION_COMMAND,"(int environmentHandle,int objectHandle,int relativeToObjectHandle,float[3] position,float[] eulerOrQuaternion)"),LUA_SETOBJECTTRANSFORMATION_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETOBJECTMATRIX_COMMAND_PLUGIN,strConCat("float[12] matrix=",LUA_GETOBJECTMATRIX_COMMAND,"(int environmentHandle,int objectHandle,int relativeToObjectHandle)"),LUA_GETOBJECTMATRIX_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_SETOBJECTMATRIX_COMMAND_PLUGIN,strConCat("",LUA_SETOBJECTMATRIX_COMMAND,"(int environmentHandle,int objectHandle,int relativeToObjectHandle,float[12] matrix)"),LUA_SETOBJECTMATRIX_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_COMPUTEJACOBIAN_COMMAND_PLUGIN,strConCat("float[] jacobian,float[] errorVector=",LUA_COMPUTEJACOBIAN_COMMAND,"(int environmentHandle,int baseObject,int lastJoint,int constraints,float[12] tipMatrix,float[12] targetMatrix=nil,int altBase=-1)"),LUA_COMPUTEJACOBIAN_CALLBACK);

    simRegisterScriptVariable("simIK.handleflag_tipdummy@simExtIK",std::to_string(ik_handleflag_tipdummy).c_str(),0);
    simRegisterScriptVariable("simIK.objecttype_joint@simExtIK",std::to_string(ik_objecttype_joint).c_str(),0);
    simRegisterScriptVariable("simIK.objecttype_dummy@simExtIK",std::to_string(ik_objecttype_dummy).c_str(),0);
    simRegisterScriptVariable("simIK.jointmode_passive@simExtIK",std::to_string(ik_jointmode_passive).c_str(),0);
    simRegisterScriptVariable("simIK.jointmode_ik@simExtIK",std::to_string(ik_jointmode_ik).c_str(),0);
    simRegisterScriptVariable("simIK.jointtype_revolute@simExtIK",std::to_string(ik_jointtype_revolute).c_str(),0);
    simRegisterScriptVariable("simIK.jointtype_prismatic@simExtIK",std::to_string(ik_jointtype_prismatic).c_str(),0);
    simRegisterScriptVariable("simIK.jointtype_spherical@simExtIK",std::to_string(ik_jointtype_spherical).c_str(),0);
    simRegisterScriptVariable("simIK.handle_all@simExtIK",std::to_string(ik_handle_all).c_str(),0);
    simRegisterScriptVariable("simIK.handle_parent@simExtIK",std::to_string(ik_handle_parent).c_str(),0);
    simRegisterScriptVariable("simIK.handle_world@simExtIK",std::to_string(ik_handle_world).c_str(),0);
    simRegisterScriptVariable("simIK.constraint_x@simExtIK",std::to_string(ik_constraint_x).c_str(),0);
    simRegisterScriptVariable("simIK.constraint_y@simExtIK",std::to_string(ik_constraint_y).c_str(),0);
    simRegisterScriptVariable("simIK.constraint_z@simExtIK",std::to_string(ik_constraint_z).c_str(),0);
    simRegisterScriptVariable("simIK.constraint_alpha_beta@simExtIK",std::to_string(ik_constraint_alpha_beta).c_str(),0);
    simRegisterScriptVariable("simIK.constraint_gamma@simExtIK",std::to_string(ik_constraint_gamma).c_str(),0);
    simRegisterScriptVariable("simIK.constraint_position@simExtIK",std::to_string(ik_constraint_position).c_str(),0);
    simRegisterScriptVariable("simIK.constraint_orientation@simExtIK",std::to_string(ik_constraint_orientation).c_str(),0);
    simRegisterScriptVariable("simIK.constraint_pose@simExtIK",std::to_string(ik_constraint_pose).c_str(),0);
    simRegisterScriptVariable("simIK.method_pseudo_inverse@simExtIK",std::to_string(ik_method_pseudo_inverse).c_str(),0);
    simRegisterScriptVariable("simIK.method_damped_least_squares@simExtIK",std::to_string(ik_method_damped_least_squares).c_str(),0);
    simRegisterScriptVariable("simIK.method_jacobian_transpose@simExtIK",std::to_string(ik_method_jacobian_transpose).c_str(),0);
    simRegisterScriptVariable("simIK.method_undamped_pseudo_inverse@simExtIK",std::to_string(ik_method_undamped_pseudo_inverse).c_str(),0);
    simRegisterScriptVariable("simIK.result_not_performed@simExtIK",std::to_string(ik_result_not_performed).c_str(),0);
    simRegisterScriptVariable("simIK.result_success@simExtIK",std::to_string(ik_result_success).c_str(),0);
    simRegisterScriptVariable("simIK.result_fail@simExtIK",std::to_string(ik_result_fail).c_str(),0);

    // deprecated:
    simRegisterScriptCallbackFunction(LUA_GETJACOBIAN_COMMAND_PLUGIN,nullptr,LUA_GETJACOBIAN_CALLBACK);
    simRegisterScriptCallbackFunction(LUA_GETMANIPULABILITY_COMMAND_PLUGIN,nullptr,LUA_GETMANIPULABILITY_CALLBACK);

    ikSetLogCallback(_logCallback);

    #ifdef _WIN32
        InitializeCriticalSection(&_simpleMutex);
    #else
        pthread_mutex_init(&_simpleMutex,0);
    #endif

    _allEnvironments=new CEnvCont();

    return(2); // 2 since V4.3.0
}

SIM_DLLEXPORT void simEnd()
{
    delete _allEnvironments;
#ifdef _WIN32
    DeleteCriticalSection(&_simpleMutex);
#else
    pthread_mutex_destroy(&_simpleMutex);
#endif
}

SIM_DLLEXPORT void* simMessage(int message,int* auxiliaryData,void*,int*)
{
    if (message==sim_message_eventcallback_scriptstatedestroyed)
    {
        int env=_allEnvironments->removeOneFromScriptHandle(auxiliaryData[0]);
        while (env>=0)
        {
            if (ikSwitchEnvironment(env))
                ikEraseEnvironment();
            env=_allEnvironments->removeOneFromScriptHandle(auxiliaryData[0]);
        }
    }

    if (message==sim_message_eventcallback_instancepass)
    {
        int consoleV=sim_verbosity_none;
        int statusbarV=sim_verbosity_none;
        simGetModuleInfo("IK",sim_moduleinfo_verbosity,nullptr,&consoleV);
        simGetModuleInfo("IK",sim_moduleinfo_statusbarverbosity,nullptr,&statusbarV);
        if ( (consoleV>sim_verbosity_none)||(statusbarV>sim_verbosity_none) )
        {
            if ( (consoleV>=sim_verbosity_trace)||(statusbarV>=sim_verbosity_trace) )
                ikSetVerbosity(5);
            else
            {
                if ( (consoleV>=sim_verbosity_debug)||(statusbarV>=sim_verbosity_debug) )
                    ikSetVerbosity(4);
                else
                    ikSetVerbosity(1);
            }
        }
        else
            ikSetVerbosity(0);
    }
    return(nullptr);
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------

SIM_DLLEXPORT int ikPlugin_createEnv()
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    int retVal=-1;
    ikCreateEnvironment(&retVal,true);
    return(retVal);
}

/*
SIM_DLLEXPORT bool ikPlugin_switchEnvironment(int handle)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    return(ikSwitchEnvironment(handle,true));
}
*/

SIM_DLLEXPORT void ikPlugin_eraseEnvironment(int ikEnv)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikEraseEnvironment();
}

SIM_DLLEXPORT void ikPlugin_eraseObject(int ikEnv,int objectHandle)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikEraseObject(objectHandle);
}

SIM_DLLEXPORT void ikPlugin_setObjectParent(int ikEnv,int objectHandle,int parentObjectHandle)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetObjectParent(objectHandle,parentObjectHandle,false);
}

SIM_DLLEXPORT int ikPlugin_createDummy(int ikEnv)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    int retVal=-1;
    if (ikSwitchEnvironment(ikEnv,true))
        ikCreateDummy(nullptr,&retVal);
    return(retVal);
}

SIM_DLLEXPORT void ikPlugin_setLinkedDummy(int ikEnv,int dummyHandle,int linkedDummyHandle)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetLinkedDummy(dummyHandle,linkedDummyHandle);
}

SIM_DLLEXPORT int ikPlugin_createJoint(int ikEnv,int jointType)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    int retVal=-1;
    if (ikSwitchEnvironment(ikEnv,true))
        ikCreateJoint(nullptr,jointType,&retVal);
    return(retVal);
}

SIM_DLLEXPORT void ikPlugin_setJointMode(int ikEnv,int jointHandle,int jointMode)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetJointMode(jointHandle,jointMode);
}

SIM_DLLEXPORT void ikPlugin_setJointInterval(int ikEnv,int jointHandle,bool cyclic,const float* intervalMinAndRange)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
    {
        double v[2]={double(intervalMinAndRange[0]),double(intervalMinAndRange[1])};
        ikSetJointInterval(jointHandle,cyclic,v);
    }
}

SIM_DLLEXPORT void ikPlugin_setJointScrewPitch(int ikEnv,int jointHandle,float pitch)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetJointScrewPitch(jointHandle,double(pitch));
}

SIM_DLLEXPORT void ikPlugin_setJointIkWeight(int ikEnv,int jointHandle,float ikWeight)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetJointIkWeight(jointHandle,double(ikWeight));
}

SIM_DLLEXPORT void ikPlugin_setJointMaxStepSize(int ikEnv,int jointHandle,float maxStepSize)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetJointMaxStepSize(jointHandle,double(maxStepSize));
}

SIM_DLLEXPORT void ikPlugin_setJointDependency(int ikEnv,int jointHandle,int dependencyJointHandle,float offset,float mult)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetJointDependency(jointHandle,dependencyJointHandle,double(offset),double(mult));
}

SIM_DLLEXPORT float ikPlugin_getJointPosition(int ikEnv,int jointHandle)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    double p=double(0.0);
    if (ikSwitchEnvironment(ikEnv,true))
        ikGetJointPosition(jointHandle,&p);
    return(float(p));
}

SIM_DLLEXPORT void ikPlugin_setJointPosition(int ikEnv,int jointHandle,float position)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetJointPosition(jointHandle,double(position));
}

SIM_DLLEXPORT void ikPlugin_getSphericalJointQuaternion(int ikEnv,int jointHandle,float quaternion[4])
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    C7Vector tr;
    tr.setIdentity();
    if (ikSwitchEnvironment(ikEnv,true))
        ikGetJointTransformation(jointHandle,&tr);
    quaternion[0]=float(tr.Q(0));
    quaternion[1]=float(tr.Q(1));
    quaternion[2]=float(tr.Q(2));
    quaternion[3]=float(tr.Q(3));
}

SIM_DLLEXPORT void ikPlugin_setSphericalJointQuaternion(int ikEnv,int jointHandle,const float* quaternion)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    C4Vector q;
    q(0)=double(quaternion[0]);
    q(1)=double(quaternion[1]);
    q(2)=double(quaternion[2]);
    q(3)=double(quaternion[3]);
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetSphericalJointQuaternion(jointHandle,&q);
}

SIM_DLLEXPORT int ikPlugin_createIkGroup(int ikEnv)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    int retVal=-1;
    if (ikSwitchEnvironment(ikEnv,true))
        ikCreateIkGroup(nullptr,&retVal);
    return(retVal);
}

SIM_DLLEXPORT void ikPlugin_eraseIkGroup(int ikEnv,int ikGroupHandle)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikEraseIkGroup(ikGroupHandle);
}

SIM_DLLEXPORT void ikPlugin_setIkGroupFlags(int ikEnv,int ikGroupHandle,int flags)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetIkGroupFlags(ikGroupHandle,flags);
}

SIM_DLLEXPORT void ikPlugin_setIkGroupCalculation(int ikEnv,int ikGroupHandle,int method,float damping,int maxIterations)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetIkGroupCalculation(ikGroupHandle,method,double(damping),maxIterations);
}

SIM_DLLEXPORT int ikPlugin_addIkElement(int ikEnv,int ikGroupHandle,int tipHandle)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    int retVal=-1;
    if (ikSwitchEnvironment(ikEnv,true))
        ikAddIkElement(ikGroupHandle,tipHandle,&retVal);
    return(retVal);
}

SIM_DLLEXPORT void ikPlugin_eraseIkElement(int ikEnv,int ikGroupHandle,int ikElementHandle)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikEraseIkElement(ikGroupHandle,ikElementHandle);
}

SIM_DLLEXPORT void ikPlugin_setIkElementFlags(int ikEnv,int ikGroupHandle,int ikElementHandle,int flags)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetIkElementFlags(ikGroupHandle,ikElementHandle,flags);
}

SIM_DLLEXPORT void ikPlugin_setIkElementBase(int ikEnv,int ikGroupHandle,int ikElementHandle,int baseHandle,int constraintsBaseHandle)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetIkElementBase(ikGroupHandle,ikElementHandle,baseHandle,constraintsBaseHandle);
}

SIM_DLLEXPORT void ikPlugin_setIkElementConstraints(int ikEnv,int ikGroupHandle,int ikElementHandle,int constraints)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetIkElementConstraints(ikGroupHandle,ikElementHandle,constraints);
}

SIM_DLLEXPORT void ikPlugin_setIkElementPrecision(int ikEnv,int ikGroupHandle,int ikElementHandle,float linearPrecision,float angularPrecision)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetIkElementPrecision(ikGroupHandle,ikElementHandle,double(linearPrecision),double(angularPrecision));
}

SIM_DLLEXPORT void ikPlugin_setIkElementWeights(int ikEnv,int ikGroupHandle,int ikElementHandle,float linearWeight,float angularWeight)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetIkElementWeights(ikGroupHandle,ikElementHandle,double(linearWeight),double(angularWeight));
}

SIM_DLLEXPORT int ikPlugin_handleIkGroup(int ikEnv,int ikGroupHandle)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    int retVal=-1;
    if (ikSwitchEnvironment(ikEnv,true))
        ikHandleIkGroup(ikGroupHandle,&retVal);
    return(retVal);
}

SIM_DLLEXPORT bool ikPlugin_computeJacobian(int ikEnv,int ikGroupHandle,int options)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    bool retVal=false;
    if (ikSwitchEnvironment(ikEnv,true))
        ikComputeJacobian_old(ikGroupHandle,options,&retVal);
    return(retVal);
}

SIM_DLLEXPORT float* ikPlugin_getJacobian(int ikEnv,int ikGroupHandle,int* matrixSize)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    float* retVal=nullptr;
    if (ikSwitchEnvironment(ikEnv,true))
    {
        size_t ms[2];
        double* m=ikGetJacobian_old(ikGroupHandle,ms);
        if (m!=nullptr)
        {
            matrixSize[0]=int(ms[0]);
            matrixSize[1]=int(ms[1]);
            retVal=reinterpret_cast<float*>(simCreateBuffer(int(sizeof(float)*ms[0]*ms[1])));
            for (size_t i=0;i<ms[0]*ms[1];i++)
                retVal[i]=float(m[i]);
            ikReleaseBuffer(m);
        }
    }
    return(retVal);
}

SIM_DLLEXPORT float ikPlugin_getManipulability(int ikEnv,int ikGroupHandle)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    double retVal=double(0.0);
    if (ikSwitchEnvironment(ikEnv,true))
        ikGetManipulability_old(ikGroupHandle,&retVal);
    return(float(retVal));
}

SIM_DLLEXPORT void ikPlugin_getObjectLocalTransformation(int ikEnv,int objectHandle,float* pos,float* quat)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    C7Vector tr;
    tr.setIdentity();
    if (ikSwitchEnvironment(ikEnv,true))
        ikGetObjectTransformation(objectHandle,ik_handle_parent,&tr);
    pos[0]=float(tr.X(0));
    pos[1]=float(tr.X(1));
    pos[2]=float(tr.X(2));
    quat[0]=float(tr.Q(0));
    quat[1]=float(tr.Q(1));
    quat[2]=float(tr.Q(2));
    quat[3]=float(tr.Q(3));
}

SIM_DLLEXPORT void ikPlugin_setObjectLocalTransformation(int ikEnv,int objectHandle,const float* pos,const float* quat)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    C7Vector tr;
    tr.X(0)=double(pos[0]);
    tr.X(1)=double(pos[1]);
    tr.X(2)=double(pos[2]);
    tr.Q(0)=double(quat[0]);
    tr.Q(1)=double(quat[1]);
    tr.Q(2)=double(quat[2]);
    tr.Q(3)=double(quat[3]);
    if (ikSwitchEnvironment(ikEnv,true))
        ikSetObjectTransformation(objectHandle,ik_handle_parent,&tr);
}

static size_t _validationCallback_jointCnt;
static bool(*__validationCallback)(float*);
static float* _validationCallback_config;

bool _validationCallback(double* conf)
{
    for (size_t i=0;i<_validationCallback_jointCnt;i++)
        _validationCallback_config[i]=float(conf[i]);
    return(__validationCallback(_validationCallback_config));
}

SIM_DLLEXPORT char* ikPlugin_getConfigForTipPose(int ikEnv,int ikGroupHandle,int jointCnt,const int* jointHandles,float thresholdDist,int maxIterations,int* result,float* retConfig,const float* metric,bool(*validationCallback)(float*),const int* jointOptions,const float* lowLimits,const float* ranges)
{
    CLockInterface lock; // actually required to correctly support CoppeliaSim's old GUI-based IK
    char* retVal=nullptr;
    if (ikSwitchEnvironment(ikEnv,true))
    {
        std::vector<double> _retConfig;
        _retConfig.resize(size_t(jointCnt));
        double* _metric=nullptr;
        double __metric[4];
        if (metric!=nullptr)
        {
            __metric[0]=double(metric[0]);
            __metric[1]=double(metric[1]);
            __metric[2]=double(metric[2]);
            __metric[3]=double(metric[3]);
            _metric=__metric;
        }
        double* _lowLimits=nullptr;
        std::vector<double> __lowLimits;
        if (lowLimits!=nullptr)
        {
            for (size_t i=0;i<size_t(jointCnt);i++)
                __lowLimits.push_back(double(lowLimits[i]));
            _lowLimits=&__lowLimits[0];
        }
        double* _ranges=nullptr;
        std::vector<double> __ranges;
        if (ranges!=nullptr)
        {
            for (size_t i=0;i<size_t(jointCnt);i++)
                __ranges.push_back(double(ranges[i]));
            _ranges=&__ranges[0];
        }
        _validationCallback_jointCnt=size_t(jointCnt);
        std::vector<float> __valCb_j;
        __valCb_j.resize(size_t(jointCnt));
        _validationCallback_config=&__valCb_j[0];
        __validationCallback=validationCallback;
        bool(*_validationCb)(double*)=nullptr;
        if (validationCallback!=nullptr)
            _validationCb=_validationCallback;
        result[0]=ikGetConfigForTipPose(ikGroupHandle,size_t(jointCnt),jointHandles,double(thresholdDist),maxIterations,&_retConfig[0],_metric,_validationCb,jointOptions,_lowLimits,_ranges);
        if (result[0]>0)
        {
            for (size_t i=0;i<_retConfig.size();i++)
                retConfig[i]=float(_retConfig[i]);
        }
        if (result[0]<0)
        {
            std::string err(ikGetLastError());
            retVal=simCreateBuffer(int(err.size()+1));
            for (size_t i=0;i<err.size();i++)
                retVal[i]=err[i];
            retVal[err.size()]=0;
        }
    }
    return(retVal);
}
