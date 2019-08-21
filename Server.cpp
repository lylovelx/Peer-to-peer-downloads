#include<boost/filesystem.hpp>
#include<iostream>
#include"httplib.h"
namespace bf = boost::filesystem;

#define SHARE_PATH "Share"
using namespace httplib;
using namespace std;
class P2PServer {

public:
	//附近主机配对请求处理
	static void PairHandler(const Request& req, Response& rsp) {
		//服务端主机所有网卡接受配对请求
		rsp.status = 200;
	}
	//文件列表请求处理
	static void ListHandler(const Request& req, Response& rsp) {
		bf::directory_iterator item_begin(SHARE_PATH);
		bf::directory_iterator item_end;
		while (item_begin != item_end) {
			//判断当前文件是否为文件夹
			if (bf::is_directory(item_begin->status())) {
				continue;
			}
			//获取文件路径
			bf::path p = item_begin->path();
			//获取文件名称
			std::string name = p.filename().string();
			rsp.body += name + "\n";
			item_begin++;
		}
		rsp.status = 200;
	}

	static bool RangeParse(string& range_val, int64_t& start, int64_t& len) {

		size_t pos1 = range_val.find("=");
		size_t pos2 = range_val.find("-");
		//bytes=start-end
		if (pos1 == string::npos || pos2 == string::npos) {
			cout << "range_val error\n";
			return false;
		}
		int64_t end;
		string rstart, rend;
		rstart = range_val.substr(pos1 + 1, pos2 - pos1 - 1);
		rend = range_val.substr(pos2 + 1);
		stringstream tmp;
		tmp << rstart;
		tmp >> start;
		tmp.clear();
		tmp << rend;
		tmp >> end;
		len = start - end + 1;
		return true;
	}


	//文件下载请求处理
	static void DownloadHandler(const Request& req, Response& rsp) {
		//得到文件路径        
		bf::path p(req.path);
		//得到文件路径
		std::string tmp = "/";
		std::string name = SHARE_PATH + tmp + p.filename().string();
		//将此路径下的文件以二进制的方式写入到内存当中a
		if (!bf::exists(name.c_str())) {
			rsp.status = 404;
			return;
			if (bf::is_directory(name.c_str())) {
				//没有操作权限
				rsp.status = 403;
				return;
			}
			int64_t fsize = bf::file_size(name.c_str());
			if (req.method == "HEAD") {
				rsp.status = 200;
				string len = to_string(fsize);
				rsp.set_header("Content-Length", len.c_str());
				return;
			}
			else {
				if (!req.has_header("Range")) {
					rsp.status = 400;
					return;
				}
				string range_val;

				range_val = req.get_header_value("Range");
				int64_t start, rlen;
				bool ret = RangeParse(range_val, start, rlen);
				if (ret == false) {
					rsp.status = 400;
					return;
				}
				std::ifstream file(name.c_str(), std::ios::binary);

				//判断文件是否正常打开
				if (!file.is_open()) {
					rsp.status = 404;
					return;
				}
				else {

					file.seekg(start, std::ios::beg);

					//給body开空间
					rsp.body.resize(rlen);
					//将内存中的文件写入到body当中
					file.read(&rsp.body[0], rlen);
					//判断是否读取成功
					if (!file.good()) {
						//服务器内部错误
						rsp.status = 500;
						return;
					}
					else {
						//文件下载
						file.close();
						rsp.status = 206;
						//关闭文件
						rsp.set_header("content-type", "aplication/octet-stream");
					}
				}
			}
		}
	}
	void Start(int port) {

		srv.bind_to_any_port("0.0.0.0");
		srv.Get("/hostpair", PairHandler);
		srv.Get("/list", ListHandler);
		srv.Get("/list/(.*)", DownloadHandler);
		srv.listen("0.0.0.0", port);

	}

private:
	Server srv;
};

int main() {
  P2PServer p;
	p.Start(9009);
	return 0;
}
