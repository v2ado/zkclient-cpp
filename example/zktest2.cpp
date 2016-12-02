#include "ZkAccept.h"
#include "ZkRequest.h"
#include <iostream>
#include <string>

using namespace std;


int main()
{
	string serstring("192.168.1.226:2181");
	string cname("cms");
	string addr("dqc@192.168.1.226:10001");
    long int ver = 1;

	ZkAccept cmsZR(cname,addr,serstring,ver);
	cmsZR.serRegister();
	sleep(1);
	ZkRequest puZR(cname,serstring,ver);
	puZR.discovery();

	string IP;
	int port = 0;
	puZR.getServer(IP,port);
	cout <<"ip  ="<<IP<<" port ="<<port<<endl;
	return 0;

}


