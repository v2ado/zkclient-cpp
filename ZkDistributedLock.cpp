/*
 *  Author: GuangJie Qu <qgjie456@163.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, 
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ZkDistributedLock.h"
#include <algorithm>
#include <list>
#include <pthread.h>
#include "boost/shared_ptr.hpp"
#include "ZkClient.h"
#include "ZkException.h"

using  boost::shared_ptr;
using  namespace std;



class watchDLockNode : public IZkDataListener
{
public:
	watchDLockNode(shared_ptr<ZkClient> cli, shared_ptr<ZkDistributedLock>plock, string &path):IZkDataListener(path)
	{
		pZklock = plock;  
	}
	~watchDLockNode(){}
	void handleDataChange(const string &dataPath, string &data)
	{}
	void handleDataCreate(const string &dataPath){}
	void   handleDataDeleted(const string &dataPath){
		pthread_mutex_lock(&(pZklock->DLockmutex));
		pZklock->DLockStats = 0;
		pthread_cond_broadcast(&(pZklock->DLockcond));

		pthread_mutex_unlock(&(pZklock->DLockmutex));
	}
	shared_ptr<ZkDistributedLock> pZklock;
};

ZkDistributedLock::ZkDistributedLock(const string &name, const string &serstring):ZkBase(name, serstring)
{
	init();
	_dLockPath = getDLockPath();
	_dLockList.clear();
	_dLockName = name;
	_dLockNoCreate = 0;
	_dLockFullPath.clear();
	_dLockNodeName.clear();
 	DLockStats = 1;

 	pthread_mutex_init(&DLockmutex, NULL);
	pthread_cond_init(&DLockcond, NULL);
}

bool ZkDistributedLock::IsNeedCreate()
{
	_dLockFullPath = _dLockPath+"/"+_dLockName;
	bool ret = false;
	try{
		ret = _zkclient->exists(_dLockFullPath,false);
		if (ret)
		{
			_dLockNoCreate= 1;
			return true;
		}
	} catch(ZkExceptionNoNode &e) {
		return false;
	}
	return false;
}

string ZkDistributedLock::findMinLock()
{
	string cur;
	string minLock;
	string tmp;
	list <string>::iterator it;

	_dLockList.sort();
	for (it = _dLockList.begin();it!=_dLockList.end();++it)
	{
		 cur = (*it);
		 tmp = _dLockFullPath + "/" +cur;
		 if (tmp ==_dLockNodeName)
		 {
		 	if (it == _dLockList.begin())
		 	{
				minLock = _dLockNodeName;
				break;  
			}
		 	--it; 
			cur = (*it);
			minLock =_dLockFullPath+"/" +cur;
			break;
		 }
	}
	return minLock;
}

bool ZkDistributedLock::Dlock()
{
	if (!_dLockNoCreate)
	{
		if (!IsNeedCreate())
		{
			_zkclient->createPersistent(_dLockFullPath,true);
		}
	}
	string lockPath = _dLockFullPath + "/";
	_dLockNodeName=_zkclient->createEphemeralSequential(lockPath,NULL);
	//cout << "ZkDistributedLock::Dlock: " << _dLockNodeName << endl;
step1:
	_dLockList = _zkclient->getChildren(_dLockFullPath, false);
	string minlock = findMinLock();
	if (minlock == _dLockNodeName)
	{
		return true;
	}

	shared_ptr<watchDLockNode> mywd(new watchDLockNode(_zkclient,shared_from_this(),  minlock));
	try{
		_zkclient->subscribeDataChanges(minlock, mywd);
		//cout << "ZkDistributedLock::subscribeDataChanges: " << minlock << endl;
	}catch (ZkExceptionNoNode &e){
		cout <<"bool ZkDistributedLock::Dlock():::subscribeDataChanges"<<e.what()<<endl;
		_zkclient->deleteRecursive(_dLockNodeName);
		return false; 
	}
	pthread_mutex_lock(&DLockmutex);

	while (DLockStats)
	{
		pthread_cond_wait(&DLockcond,&DLockmutex);
	}
	DLockStats =1;
	_zkclient->unsubscribeDataChanges(minlock, mywd);

	pthread_mutex_unlock(&DLockmutex);
	goto step1;
}

bool ZkDistributedLock::unDlock()
{
	cout << "ZkDistributedLock::unDlock: " << _dLockNodeName << endl;
	try{
	_zkclient->deleteRecursive(_dLockNodeName);
	}catch (ZkExceptionNoNode &e){
	}catch (ZkException &e){
	}
	return true;
}
bool ZkDistributedLock::waitDlock(long timeout)
{
	if (!_dLockNoCreate)
	{
		if (!IsNeedCreate())
		{
			_zkclient->createPersistent(_dLockFullPath,true);
		}
	}
	string lockPath = _dLockFullPath + "/";
	_dLockNodeName=_zkclient->createEphemeralSequential(lockPath,NULL);

	_dLockList= _zkclient->getChildren(_dLockFullPath, false);
	string minlock = findMinLock();
	if (minlock == _dLockNodeName)
	{
		return true;
	}
	
	shared_ptr<watchDLockNode> mywd(new watchDLockNode(_zkclient, shared_from_this(), minlock));
	try{
		_zkclient->subscribeDataChanges(minlock, mywd);
	}catch (ZkExceptionNoNode &e){
		cout <<"bool ZkDistributedLock::waitDlock():::subscribeDataChanges"<<e.what()<<endl;
	}
	pthread_mutex_lock(&DLockmutex);
	struct timeval  now;
	gettimeofday(&now, NULL);
	while (DLockStats)
	{
		struct timespec mytimeout;
		mytimeout.tv_nsec = now.tv_usec *1000;
		mytimeout.tv_sec =  now.tv_sec + timeout;
		pthread_cond_timedwait(&DLockcond,&DLockmutex,&mytimeout);
	}
	DLockStats =1;
	_zkclient->unsubscribeDataChanges(minlock, mywd);
	pthread_mutex_unlock(&DLockmutex);

	_dLockList= _zkclient->getChildren(_dLockFullPath, false);
	minlock = findMinLock();
	if (minlock == _dLockNodeName)
	{
		return true;
	}
	_zkclient->deleteRecursive(_dLockNodeName);
	return false;
 }


