#include<fstream>
#include<vector>
#include<thread>
#include<string>
#include<unistd.h>
#include<iostream>
#include<ifaddrs.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<boost/thread.hpp>
#include<arpa/inet.h>
#include<unistd.h>
#include<fcntl.h>
#include"httplib.h"
#include<boost/filesystem.hpp>
#include<boost/algorithm/string.hpp>

#define RANGE_SIZE (100)
namespace bf = boost::filesystem;
using namespace std;
using namespace httplib;
class P2PClient {

public:
	void ShowOnlineHost() {
		struct ifaddrs *addrs, *ifa = NULL;
		//获取本机网卡信息
		getifaddrs(&addrs);
		//循环获取本机所有的网卡信息
		for (ifa = addrs; ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr == NULL) {
				continue;
			}
			//进行类型强转
			struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
			struct sockaddr_in* mask = (struct sockaddr_in*)ifa->ifa_netmask;
			//本地回环网卡除外
			//inet_addr 将点分十进制的字符串IP地址转化成32位的网络字节序IP地址
			if (addr->sin_addr.s_addr == inet_addr("127.0.0.1")) {
				continue;
			}
			if (addr->sin_addr.s_addr == inet_addr("192.168.122.1")) {
				continue;
			}
			//将网络字节序转换位主机字节序
			if (addr->sin_family == AF_INET) {
				uint32_t ip = ntohl(addr->sin_addr.s_addr);
				uint32_t mk = ntohl(mask->sin_addr.s_addr);
				//求出网络号和主机位数
				uint32_t network = ip & mk;
				uint32_t num = ~mk;
				uint32_t i;
				for (i = 1; i < num; ++i) {
					uint32_t ip_host = network + i;
					uint32_t ip_net = htonl(ip_host);
					char ip_buffer[32];
					inet_ntop(AF_INET, &ip_net, ip_buffer, 32);
					_host_list.push_back(ip_buffer);
				}
			}
			int i = 0;
			for (auto& e : _host_list) {
				std::cout << i++ << ": " << e << std::endl;
			}
		}
		freeifaddrs(addrs);
	}
	void HostPair(string& e) {

		Client client(&e[0], 9009);
		auto rsp = client.Get("/hostpair");
		if (rsp && rsp->status == 200) {
			_online_list.push_back(e);
		}

	}
	void PairNearbyHost() {
		_online_list.clear();
		vector<thread> thr_list(_host_list.size());
		for (size_t i = 1; i < _host_list.size(); ++i) {
			thread thr(&P2PClient::HostPair, this, ref(_host_list[i]));
			thr_list[i] = move(thr);
		}
		for (size_t i = 0; i < thr_list.size(); ++i) {
			thr_list[i].join();
		}
	}
	void ShowNearbyHost() {
		int i = 0;
		for (auto& e : _online_list) {
			std::cout << i++ << ":"<< e << std::endl;
		}
	}
	//显示指定主机的文件列表
	void GetShareList() {
		std::cout << "输入你要获取的主机号：";
		fflush(stdout);
		std::cin >> _host_id;
		Client client(_online_list[_host_id].c_str(), 9009);
		auto rsp = client.Get("/list");
		std::string filename;
		boost::split(_file_list, rsp->body, boost::is_any_of("\n"));
		int i = 0;
		for (auto& e : _file_list) {
    std::cout << i++ << ":" << e << std::endl;
		}
	}
	int64_t GetFileSize() {

		int64_t fsize = -1;

		std::cout << "输入你要下载的文件编号:\n";
		
		std::cin >> _file_id;
		string uri = "/list/" + _file_list[_file_id];
		Client client(_online_list[_host_id].c_str(), 9009);
		auto rsp = client.Head(uri.c_str());
		if (rsp && rsp->status == 200) {
			if (!rsp->has_header("Content_Length")) {
				return -1;
			}
			string len = rsp->get_header_value("Content-Length");
			stringstream tmp;
			tmp << len;
			tmp >> fsize;
		}
		return fsize;
	}
	void RangeDownLoad(int64_t start, int64_t end, int* res) {
		string uri = "/list/" + _file_list[_file_id];
		string realpath = "DownLoad" + _file_list[_file_id];
		stringstream range_val;
		range_val << "bytes=" << start << "-" << end;
		Client client(uri.c_str(), 9009);
		Headers header;
		header.insert(make_pair("Range", range_val.str().c_str()));
		auto rsp = client.Get(uri.c_str(), header);
		if (rsp && rsp->status == 206) {
			int fd = open(realpath.c_str(), std::ios::binary);
			lseek(fd, start, SEEK_SET);
			int ret = write(fd, &rsp->body[0], rsp->body.size());
			if (ret < 0) {
				close(fd);
				return;
			}
			close(fd);
			*res = true;
		}
	}
	void DownloadFile() {
		int64_t fsize = GetFileSize();
		if (fsize < 0) {
			cout << "Download failure\n";
			return;
		}
		int count = fsize / RANGE_SIZE;
		vector<boost::thread> thr_list(count + 1);
		vector<int> res_list(count + 1);
		for (int i = 0; i <= count; ++i) {
			int64_t start, end, rlen;
			start = i * RANGE_SIZE;
			end = (i + 1)*RANGE_SIZE - 1;
			if (count == i) {
				if (fsize % RANGE_SIZE == 0) {
					break;
				}
				end = fsize - 1;
			}
			rlen = end - start + 1;
			//Range : bytes = start-end;
			boost::thread thr(&P2PClient::RangeDownLoad, this, start, end, &res_list[i]);
			thr_list[i] = move(thr);
		}
		bool ret = true;
		for (int i = 0; i <= count; ++i) {
			if (i == count && fsize % RANGE_SIZE == 0) {
				break;
			}
			thr_list[i].join();
			if (res_list[i] == false) {
				ret = false;
			}
		}
		if (ret == true) {

			cout << "download success\n";
		}
		else {

			cout << "download error\n";
		}
	}


	void Start() {
		while (1) {

			std::cout << "1.获取局域网中所有主机\n";
			std::cout << "2.进行主机匹配\n";
			std::cout << "3.获取指定主机的的文件列表\n";
			std::cout << "4.指定主机的文件下载\n";
			int id;
			std::cin >> id;
			switch (id) {
			case 1:
				ShowOnlineHost();
				break;
			case 2:
				ShowNearbyHost();
				break;
			case 3:
				GetShareList();
				DownloadFile();
				break;
			}

		}
	}
private:
	int _host_id;
  int _file_id;
	std::vector<std::string>  _online_list;
	std::vector<std::string>  _file_list;
	std::vector<std::string>  _host_list;
};


int main() {
	P2PClient c;
	c.ShowOnlineHost();
	c.PairNearbyHost();
	c.ShowNearbyHost();
	//c.GetShareList();
	//c.Start();
	return 0;
}
