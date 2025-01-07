#include <iostream>
#include <fstream>
#include <sstream>
#include <mysql/mysql.h>
#include "httplib.h"
//#include "api.cpp"
#include "json.hpp"
#include <iomanip>
#include <codecvt>
#include <locale>
#include <cstring>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

nlohmann::json output;      // 返回结果为json对象
const int MAX_ISBNS = 1000; // 假设最多存储 100 个 ISBN 号，可根据实际需求调整
std::string isbnArray[MAX_ISBNS];
nlohmann::json book;

// 数据库连接配置
// 服务器地址
static const char *server = "82.156.207.47";
// 用户名
static const char *user = "tianci";
// 密码
static const char *password = "doozyexact";
// 数据库名
static const char *database = "test_zhu";

// 全局变量保存顺序号
int globalSeqNumber = 0;

MYSQL *connectToDatabase()
{
    // 初始化MySQL连接
    MYSQL *conn = mysql_init(nullptr);
    // 如果初始化失败
    if (!conn)
    {
        std::string Message = "初始化MySQL连接失败";
        output["status"] = "failure";
        output["message"] = Message;

        // 返回空指针
        return nullptr;
    }

    // 连接到数据库
    if (!mysql_real_connect(conn, server, user, password, database, 0, nullptr, 0))
    {
        // 输出错误信息

        std::string Message = "mysql_real_connect() failed: " + std::string(mysql_error(conn));
        output["status"] = "failure";
        output["message"] = Message;

        // 关闭数据库连接
        mysql_close(conn);
        // 返回空指针
        return nullptr;
    }

    // 返回数据库连接
    return conn;
}
// 将中文字符串转换为拼音首字母（无效果，未实现）
char getChineseAuthorInitial(const std::string &chineseName)
{
    return 'z';
}

// 从JSON文件中读取图书信息
std::pair<std::string, char> readBookInfo(const std::string &jsonFilePath)
{
    // 打开JSON文件
    std::ifstream file(jsonFilePath);
    if (!file.is_open())
    {
        // 输出错误信息
        std::string Message = std::string("无法打开文件: ") + jsonFilePath;
        output["status"] = "failure";
        output["message"] = Message;
        // std::cout << output << std::endl;
        // std::cerr << "无法打开文件: " << jsonFilePath << std::endl;
        return {"cannotopen", '4'}; // 返回值分类号和作者名字首字母
    }

    // 解析JSON数据
    nlohmann::json jsonData;
    try
    {
        file >> jsonData;
    }
    catch (const nlohmann::json::parse_error &e)
    {
        // 输出错误信息
        std::string Message = std::string("解析JSON文件失败: ") + jsonFilePath;
        output["status"] = "failure";
        output["message"] = Message;
        // std::cout << output << std::endl;
        // std::cerr << "解析JSON文件失败: " << e.what() << std::endl;
        return {"cannotalanyls", '5'};
    }

    if (jsonData.empty())
    {
        // 输出错误信息
        std::string Message = std::string("JSON文件为空") + jsonFilePath;
        output["status"] = "failure";
        output["message"] = Message;
        // std::cout << output << std::endl;
        // std::cerr << "JSON文件为空" << std::endl;
        return {"jsonempty", '6'};
    }

    // 存储分类号的字符串
    std::string genus = "null";
    // 存储作者姓名的字符串
    std::string author = "null";

    if (jsonData.contains("data") && jsonData["data"].contains("details") && !jsonData["data"]["details"].empty())
    {
        const auto &details = jsonData["data"]["details"][0];
        if (details.contains("genus"))
        {
            // 获取分类号
            genus = details["genus"];
        }
        else
        {
            // 输出错误信息
            std::string Message = std::string("JSON文件中没有分类号信息") + jsonFilePath;
            output["status"] = "failure";
            output["message"] = Message;
            // std::cout << output << std::endl;
            genus = "JSON文件中没有分类号信息";
            return {genus, '7'};
        }
        if (details.contains("author"))
        {
            // 获取作者姓名
            author = jsonData["data"]["details"][0]["author"]; // details["author"];
        }
        else
        {
            // 输出错误信息
            std::string Message = std::string("JSON文件中没有作者姓名信息") + jsonFilePath;
            output["status"] = "failure";
            output["message"] = Message;
            // std::cout << output << std::endl;
            // std::cerr << "JSON文件中没有作者姓名信息" << std::endl;
            author = "8";
            return {genus, '8'};
        }
    }
    else
    {
        // 输出错误信息
        std::string Message = std::string("JSON文件结构不正确,无法找到图书详情") + jsonFilePath;
        output["status"] = "failure";
        output["message"] = Message;
        // std::cout << output << std::endl;
        // std::cerr << "JSON文件结构不正确,无法找到图书详情" << std::endl;
        genus = "JSON文件结构不正确";
        return {genus, '9'};
    }

    // 存储作者姓名首字母的字符
    char authorInitial = '0';
    if (author != "null")
    {
        if (author[0] & 0x80)
        { // 判断是否为中文字符
            // 获取中文作者姓名的拼音首字母
            authorInitial = getChineseAuthorInitial(author);
        }
        else
        {
            // 获取英文作者姓名的首字母
            authorInitial = author[0];
        }
    }

    // 返回分类号和作者姓名首字母
    output["status"] = "success";
    output["message"] = "获取分类号和首字母成功";
    return {genus, authorInitial};
}

// 查重并增量函数
std::string checkAndIncrementBook(const std::string &isbn)
{
    // 初始化MySQL连接
    MYSQL *conn = mysql_init(nullptr);
    if (!conn)
    {
        // 如果初始化失败，返回相应信息
        std::string Message = std::string("初始化MySQL连接失败");
        output["status"] = "failure";
        output["message"] = Message;
        std::cout << output << std::endl;
        // std::cerr << "mysql_init() failed" << std::endl;
        return "mysql_init() failed";
    }

    // 连接到MySQL数据库
    if (!mysql_real_connect(conn, server, user, password, database, 0, nullptr, 0))
    {
        std::string Message = "mysql_real_connect() failed: " + std::string(mysql_error(conn));
        output["status"] = "failure";
        output["message"] = Message;
        std::cout << output << std::endl;
        // std::cerr << "mysql_real_connect() failed: " << mysql_error(conn) << std::endl;
        std::string error = "mysql_real_connect() failed: ";
        error += mysql_error(conn);
        // 关闭MySQL连接
        mysql_close(conn);
        return error;
    }

    // 获取ISBN号
    std::string isbn = book["ISBN"];

    // 检查是否存在该ISBN号的记录并获取数量
    std::string query = "SELECT totalNum FROM book_circulation WHERE ISBN = '" + isbn + "'";
    if (mysql_query(conn, query.c_str()) != 0)
    {
        std::string Message = "查询数据库流通库表失败: " + std::string(mysql_error(conn));
        output["status"] = "failure";
        output["message"] = Message;
        std::cout << output << std::endl;
        // std::cerr << "查询数据库流通库表失败: " << mysql_error(conn) << std::endl;
        std::string error = "查询数据库流通库表失败: ";
        error += mysql_error(conn);
        // 关闭MySQL连接
        mysql_close(conn);
        return error;
    }

    // 获取查询结果
    MYSQL_RES *result = mysql_store_result(conn);
    if (!result)
    {
        std::string Message = "获取查询结果失败: " + std::string(mysql_error(conn));
        output["status"] = "failure";
        output["message"] = Message;
        std::cout << output << std::endl;
        // std::cerr << "获取查询结果失败: " << mysql_error(conn) << std::endl;
        std::string error = "获取查询结果失败: ";
        error += mysql_error(conn);
        // 关闭MySQL连接
        mysql_close(conn);
        return error;
    }

    if (mysql_num_rows(result) > 0)
    {
        // 获取查询结果的第一行
        MYSQL_ROW row = mysql_fetch_row(result);
        // 将查询结果转换为整数
        int totalNum = atoi(row[0]);

        // 释放查询结果
        mysql_free_result(result);

        // 构建更新记录的SQL语句
        query = "UPDATE book_circulation SET totalNum = " + std::to_string(totalNum + 1) + " WHERE ISBN = '" + isbn + "'";
        if (mysql_query(conn, query.c_str()) != 0)
        {
            std::string Message = "更新记录失败: " + std::string(mysql_error(conn));
            output["status"] = "failure";
            output["message"] = Message;
            std::cout << output << std::endl;
            // std::cerr << "更新记录失败: " << mysql_error(conn) << std::endl;
            std::string error = "更新记录失败: ";
            error += mysql_error(conn);
            // 关闭MySQL连接
            mysql_close(conn);
            return error;
        }
        else
        {
            // 如果更新成功，返回成功信息
            std::string successMessage = "成功更新记录数量，新的数量为 " + std::to_string(totalNum + 1);
            mysql_close(conn);
            return successMessage;
        }
    }
    else
    {
        // 如果未找到记录，返回相应信息
        mysql_free_result(result);
        mysql_close(conn);
        return "NOfind";
    }
}

// 编目图书函数
std::string catalogBook(const std::string &isbn, const std::string &classificationNumber, char authorInitial)
{

    // 如果顺序号已达到最大值9999，返回相应信息
    if (globalSeqNumber > 9999)
    {

        std::string Message = "顺序号已达到9999";
        output["status"] = "failure";
        output["message"] = Message;
        return "failture";
    }

    // 生成新的编目号
    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << globalSeqNumber;
    std::string seqNumberStr = oss.str();
    std::string newCatalogNumber = classificationNumber + authorInitial + seqNumberStr;

    // 递增全局顺序号
    globalSeqNumber++;

    return newCatalogNumber;
}

int getisbn()
{
//  // 给采访子系统发送请求
//  httplib::Client cli("http://0.0.0.0:8080");
//  auto res = cli.Post("/get_isbns");

//  // std::string isbnArray[MAX_ISBNS];
//  int index = 0;
//  if (res && res->status == 200)
//  {
//      nlohmann::json isbns = nlohmann::json::parse(res->body);
//      for (const auto &isbn : isbns)
//      {
//          if (index < MAX_ISBNS)
//          {
//              isbnArray[index++] = isbn.get<std::string>();
//          }
//          else
//          {
//              std::string Message = "数组已满，无法存储更多的 ISBN 号";
//              output["status"] = "failure";
//              output["message"] = Message;
//              std::cout << output << std::endl;

//              // std::cerr << "数组已满，无法存储更多的 ISBN 号" << std::endl;
//              break;
//          }
//      }
//      return index;
//  }
//  else
//  {
//      std::string Message = "请求失败";
//      output["status"] = "failure";
//      output["message"] = Message;
//      std::cout << output << std::endl;
//      // std::cerr << "请求失败" << std::endl;
//      return index;
//  }
   // TODO: should integrate with guoli
   int index = 0;
   std::string isbn;
   isbn = "9787115426871";
   isbnArray[index++] = isbn;
   isbn = "9787111677222";
   isbnArray[index++] = isbn;
}

void createTables(MYSQL *conn)
{
    // 创建移送清单表的SQL语句
    const char *createTransferListTable = "CREATE TABLE IF NOT EXISTS TransferTable ("
                                          "isbn VARCHAR(255),"
                                          //"time DATETIME,"
                                          "time DATE,"
                                          "adminname VARCHAR(255))";

    // 执行创建移送清单表的SQL语句
    if (mysql_query(conn, createTransferListTable) != 0)
    {
        // 输出错误信息
        std::cerr << "移送清单不存在，且创建移送清单表失败: " << mysql_error(conn) << std::endl;
    }
    else
    {
        // 输出成功信息
        // std::cout << "移送清单表创建成功" << std::endl;
    }
}

std::string getCurrentTime()
{
    // 获取当前时间
    std::time_t now = std::time(nullptr);
    // 将时间转换为本地时间
    std::tm *currentTime = std::localtime(&now);

    // 格式化时间字符串，只保留年月日
    char timeStr[11]; // 因为 "yy - mm - dd" 格式最多需要 10 个字符，加上字符串结束符 '\0'
    std::strftime(timeStr, 11, "%Y-%m-%d", currentTime);
    // 返回时间字符串
    return std::string(timeStr);
}
/**
 * @brief 插入新的图书记录
 * 
 * 该函数接收一个 `nlohmann::json` 类型的参数 `book`，表示要插入的图书信息。
 * 函数首先连接到 MySQL 数据库，然后根据图书的 ISBN 号插入新的图书记录。
 * 
 * @param book 包含图书信息的 `nlohmann::json` 对象
 * @return std::string 表示操作结果的字符串
 */
std::string insertRecord(const nlohmann::json& book) {
    // 初始化MySQL连接
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        // 如果连接初始化失败，返回错误信息
        return "mysql_init() failed";
    }

    // 连接到MySQL数据库
    if (!mysql_real_connect(conn, server, user, password, database, 0, nullptr, 0)) {
        // 如果连接数据库失败，构造错误信息
        std::string error = "mysql_real_connect() failed: ";
        // 获取 MySQL 错误信息并添加到错误信息中
        error += mysql_error(conn);
        // 关闭 MySQL 连接
        mysql_close(conn);
        // 返回错误信息
        return error;
    }

    // 获取ISBN号
    std::string isbn = book["ISBN"];

    // 插入新记录
    std::stringstream ss;
    ss << "INSERT INTO book_circulation (ISBN, callNum, CLCNum, bookTitle, author, press, pressDate, introduction, leftNum, totalNum) VALUES ('"
       << isbn << "', '" << book["callNum"] << "', '" << book["CLCNum"] << "', '" << book["bookTitle"] << "', '" << book["author"] << "', '"
       << book["press"] << "', '" << book["pressDate"] << "', '" << book["introduction"] << "', 1, 1)";
    std::string query = ss.str();

    if (mysql_query(conn, query.c_str())!= 0) {
        // 如果插入新记录失败，构造错误信息
        std::string error = "插入新记录失败: ";
        // 获取 MySQL 错误信息并添加到错误信息中
        error += mysql_error(conn);
        // 关闭 MySQL 连接
        mysql_close(conn);
        // 返回错误信息
        return error;
    } else {
        // 关闭 MySQL 连接
        mysql_close(conn);
        // 返回成功插入新记录的信息
        return "success";
    }
}
void writeToTransferList(MYSQL *conn, const std::string &isbn, const std::string &adminName)
{
    // 构造插入语句
    std::string query = "INSERT INTO TransferTable (isbn, catalogcode, time, adminname) VALUES ('" + isbn + "', '" + "', '" + getCurrentTime() + "', '" + adminName + "')";
    // 执行插入语句
    if (mysql_query(conn, query.c_str()) != 0)
    {
        // 输出错误信息
        // std::cerr << "写入移送清单失败: " << mysql_error(conn) << std::endl;
    }
    else
    {
        // 输出成功信息
        // std::cout << "成功写入移送清单" << std::endl;
    }
}
void isbn_search(const char *isbn)
{
    char command[100];
    snprintf(command, sizeof(command), "nodejs ./isbn2.js %s > ./info_tianci.json", isbn);
    assert(system(command) != -1);
}

std::vector<std::string> readJsonfullInfo(const std::string &jsonFilePath)
{
    std::ifstream file(jsonFilePath);
    if (!file.is_open())
    {
        std::string Message = std::string("无法打开文件: ") + jsonFilePath;
        output["status"] = "failure";
        output["message"] = Message;
        return {};
    }

    nlohmann::json jsonData;
    try
    {
        file >> jsonData;
    }
    catch (const nlohmann::json::parse_error &e)
    {
        std::string Message = std::string("解析JSON文件失败: ") + jsonFilePath;
        output["status"] = "failure";
        output["message"] = Message;
        return {};
    }

    if (jsonData.empty())
    {
        std::string Message = std::string("JSON文件为空") + jsonFilePath;
        output["status"] = "failure";
        output["message"] = Message;
        return {};
    }

    if (jsonData.contains("data") && jsonData["data"].contains("details") && !jsonData["data"]["details"].empty())
    {
        const auto &details = jsonData["data"]["details"][0];
        std::vector<std::string> info;

        // isbn
        if (details.contains("isbn"))
        {
            info.push_back(details["isbn"]);
        }
        else
        {
            info.push_back("");
        }

        // 中图法分类号
        if (details.contains("genus"))
        {
            info.push_back(details["genus"]);
        }
        else
        {
            info.push_back("");
        }

        // 书名
        if (details.contains("title"))
        {
            info.push_back(details["title"]);
        }
        else
        {
            info.push_back("");
        }

        // 作者
        if (details.contains("author"))
        {
            info.push_back(details["author"]);
        }
        else
        {
            info.push_back("");
        }

        // 出版社
        if (details.contains("publisher"))
        {
            info.push_back(details["publisher"]);
        }
        else
        {
            info.push_back("");
        }

        // 出版日期
        if (details.contains("pubDate"))
        {
            info.push_back(details["pubDate"]);
        }
        else
        {
            info.push_back("");
        }

        // 简介
        if (details.contains("gist"))
        {
            info.push_back(details["gist"]);
        }
        else
        {
            info.push_back("");
        }

        return info;
    }
    else
    {
        std::string Message = std::string("JSON文件结构不正确,无法找到图书详情") + jsonFilePath;
        output["status"] = "failure";
        output["message"] = Message;
        return {};
    }
}

int jsonCatalog()
{
    // 连接到数据库
    MYSQL *conn = connectToDatabase();
    // 如果连接失败
    if (!conn)
    {
        // 输出错误信息
        // std::string Message = "无法连接到数据库" + std::string(mysql_error(conn));
        // output["status"] = "failure";
        // output["message"] = Message;
        std::cout << output << std::endl;
        return 1;
    }

    std::string isbn;
    // TODO: wait to integrate with guoli
    int index = getisbn(); // 获取isbn号和数量

    if (index == 0)
    {
        std::string Message = "请求未获取到isbn,请检查服务器是否运行";
        output["status"] = "failure";
        output["message"] = Message;
        std::cout << output << std::endl;
        return 1;
    }

    for (int i = 0; i < index; ++i)
    {
        isbn = isbnArray[i];

        isbn_search(isbn.c_str());
        // JSON文件路径
        std::string jsonFilePath = "./info_tianci.json";
        // 读取图书信息
        auto result = readBookInfo(jsonFilePath); // first为分类号，second为作者姓名首字母
        if ((result.second >= '0') && (result.second <= '9'))
        {
            std::cout << output << std::endl;
            return 1;
        }

        // 编目图书
        std::string catalogcode = catalogBook(isbn, result.first, result.second);

        if (catalogcode.find(result.first) != std::string::npos)
        {
        }
        else
        {
            std::string Message = "分类号获取错误";
            output["status"] = "failure";
            output["message"] = Message;
            std::cout << output << std::endl;
        }

        std::string jsonFilePath = "./info_tianci.json"; // 读取全部信息
        std::vector<std::string> info = readJsonfullInfo(jsonFilePath);
        book["callNum"] = catalogcode;
        if (!info.empty())
        {
            book["ISBN"] = info[0];
            book["CLCNum"] = info[1];
            book["bookTitle"] = info[2];
            book["author"] = info[3];
            book["press"] = info[4];
            book["pressDate"] = info[5];
            book["introduction"] = info[6];
        }
        else
        {
            std::string Message = "图书详细信息获取失败";
            output["status"] = "failure";
            output["message"] = Message;
            std::cout << output << std::endl;
            return 1;
        }
        // 检查并更新图书记录的数量
        std::string checkResult = checkAndIncrementBook(isbn);

        
        
        // 如果未找到记录，进行插入
        if (checkResult.find("Nofind") != std::string::npos || checkResult.find("success") != std::string::npos)
        {

            std::string Message = "记录存在，且成功增加";
            output["status"] = "success";
            output["message"] = Message;

            if (checkResult.find("Nofind") != std::string::npos)
            {
                std::string insert = insertRecord(book);
                std::string Message = "记录不存在，且插入成功";
                output["status"] = "success";
                output["message"] = Message;

                if (insert !="success")
                {
                    std::string Message = insert;
                    output["status"] = "failture";
                    output["message"] = Message;
                    std::cout << output << std::endl;
                    return 1;
                }

            }

        }
        else
        {
            std::string Message = checkResult;
            output["status"] = "failture";
            output["message"] = Message;
            std::cout << output << std::endl;
            return 1;
        }

        // 写入移送清单
        writeToTransferList(conn, isbn, "admin1");
    }
    std::cout << output << std::endl;
    return 1;
}