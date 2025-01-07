#define _CRT_SECURE_NO_WARNINGS 1 //sprintf有警告
#include <mysql/mysql.h>
#include <stdio.h>
#include <iostream>
#include <assert.h>
#include <string>
#include "json.hpp"
using namespace std;  

#define SQL_MAX 256		// sql语句字符数组最大值
static MYSQL* conn;
static MYSQL_RES* res;
static MYSQL_ROW row;
static const char* host = "82.156.207.47";
static const char* user = "yixuan";
static const char* passwd = "yarnimprovise";
static const char* db = "test_zhang";

int ifExit(string ISBN);
void AddNewBook(const nlohmann::json Book);
void DelBook(const nlohmann::json Book);

int GetLeftNum(const nlohmann::json Borrow);//根据ISBN从流通库表中读剩余数量ok
int GetTotalNum(const nlohmann::json Borrow);

void BorrowBook(const nlohmann::json Borrow);
int BorrowAuthority(const nlohmann::json Borrow);//ok
int AddBorrowList(const nlohmann::json Book);//ok

int DecreaseLeftNum(const nlohmann::json Borrow);//ok
int ifLeft(const nlohmann::json Borrow);//ok
int ifCredit(const nlohmann::json Borrow);//ok
int GetBorrowNum(const nlohmann::json Borrow);//根据uID从借阅记录表读在借数量（有效位为1的行数） 
int ifBelowLimit(const nlohmann::json Borrow);

void ReturnBook(const nlohmann::json Borrow);
int ifOverDue(const nlohmann::json Return);
int DelBorrowList(const nlohmann::json Borrow);
int IncreaseNum(const nlohmann::json Return);
int IncreaseLeftNum(const nlohmann::json Return);



MYSQL* connectToDB(const char* host, const char* user, const char* passwd, const char* db) {// 封装数据库连接
    MYSQL* conn = mysql_init(NULL);
    if (conn == NULL) {
        std::cerr << "MySQL initialization failed" << std::endl;
        return NULL;
    }
    if (mysql_real_connect(conn, host, user, passwd, db, 0, NULL, 0) == NULL) {
        std::cerr << "MySQL connection failed: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return NULL;
    }
    return conn;
}
void releaseDB(MYSQL* conn, MYSQL_RES* res) {// 封装数据库资源释放
    if (res != NULL) {
        mysql_free_result(res);
    }
    if (conn != NULL) {
        mysql_close(conn);
    }
}

void jsonBorrowBook(nlohmann::json Borrow) {
    conn = connectToDB(host, user, passwd, db);
    assert(conn != NULL);

    Borrow["credit"]= 1 ;//用户子系统
    Borrow["borrowLimit"] = 5;//用户子系统
    
    BorrowBook(Borrow);

    releaseDB(conn, res);
}

void jsonReturnBook(nlohmann::json Borrow) {
    conn = connectToDB(host, user, passwd, db);
    assert(conn != NULL);

    string isOver = Borrow["isOver"];
    Borrow["ifOver"]= isOver;

    Borrow["credit"]= 1 ;//用户子系统
    Borrow["borrowLimit"] = 5;//用户子系统
    
    ReturnBook(Borrow);

    releaseDB(conn, res);
}

// 辅助函数，从json对象中提取字符串
std::string extractString(const nlohmann::json& j, const std::string& key) {

    if (j.contains(key)) {
        return j[key];
    }
    return "";
}
int GetLeftNum(const nlohmann::json Borrow) {
    int left = 0;
    std::string ISBN;
    if (Borrow.contains("ISBN")) {
        ISBN = Borrow["ISBN"];
    }
    else {
        cout << "Missing required 'ISBN 'in JSON" << endl;
        return -1;
    }
    std::string sql = "SELECT leftNum FROM book_circulation WHERE ISBN = '" + ISBN + "'";
    int query_result = mysql_query(conn, sql.c_str());
    if (query_result != 0) {
        std::cerr << "mysql_query() failed: " << mysql_error(conn) << std::endl;
        return -1;
    }
    MYSQL_RES* res = mysql_store_result(conn);
    if (res == NULL) {
        std::cerr << "mysql_store_result() failed: " << mysql_error(conn) << std::endl;
        return -1;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row != NULL) {
        left = atoi(row[0]);
        mysql_free_result(res);
        return left;
    }
    else {
        std::cout << "No record found" << std::endl;
        mysql_free_result(res);
        return -1;
    }
}

int GetTotalNum(const nlohmann::json Borrow) {
    int total = 0;
    std::string ISBN;
    if (Borrow.contains("ISBN")) {
        ISBN = Borrow["ISBN"];
    }
    else {
        cout << "Missing required 'ISBN 'in JSON" << endl;
        return -1;
    }
    std::string sql = "SELECT totalNum FROM book_circulation WHERE ISBN = '" + ISBN + "'";
    int query_result = mysql_query(conn, sql.c_str());
    if (query_result != 0) {
        std::cerr << "mysql_query() failed: " << mysql_error(conn) << std::endl;
        return -1;
    }
    MYSQL_RES* result = mysql_store_result(conn);
    if (result == NULL) {
        std::cerr << "mysql_store_result() failed: " << mysql_error(conn) << std::endl;
        return -1;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row != NULL) {
        total = atoi(row[0]);
        mysql_free_result(result);
        return total;
    }
    else {
        std::cout << "No record found" << std::endl;
        mysql_free_result(result);
        return -1;
    }
}


//------------------------------------------------------借书----------------------------------------------------------------------------------------

int ifLeft(const nlohmann::json Borrow) {//查流通库表，剩余数量大于0返回1，等于0返回0
    int left = GetLeftNum(Borrow);
    
    if (left > 0) {
        return 1;
    }
    return 0;
}
//调用用户子系统提供的方法得到信任位，可信返回1，不可信返回0，出错返回2
int ifCredit(const nlohmann::json Borrow) {

    int credit = 0;
    if (Borrow.contains("credit")) {
        credit= Borrow["credit"];
    }
    else {
        cout << "Missing required 'credit'in JSON" << endl;
        credit = 2;       
    }   
    return credit;
}
//用户借书量达上限返回0，未达上限返回1，出错返回2
int ifBelowLimit(const nlohmann::json Borrow) {//调用用户子系统的方法得到借书上限，查借阅记录表valid值为1的行数为在借数量，比较判断借书量是否达上限
    int num = 0;
    int limit = 0;
    if (Borrow.contains("borrowNum")) {
        num = Borrow["borrowNum"];
    }
    else {
        cout << "Missing required 'borrowNum'in JSON" << endl;
        return 2;
    }

    if (Borrow.contains("borrowLimit")) {
        limit = Borrow["borrowLimit"];
    }
    else {
        cout << "Missing required 'borrowLimit'in JSON" << endl;
        return 2;
    }
    if (num >= limit) {
        return 0;
    }
    return 1;//未达上限返回1
}
//使用ifLeft、
int BorrowAuthority(const nlohmann::json Borrow) {
    nlohmann::json result;
    int leftnumber = ifLeft(Borrow);
    if (leftnumber == 2) {   
       result["status"] = "failure";
       result["message"] = "Error getting left number";
       std::cout << result.dump() << std::endl;        
        return 0;
    }
    if (leftnumber == 0) {
       result["status"] = "failure";
       result["message"] = "error：借书失败！当前无余量";
       std::cout << result.dump() << std::endl;
        return 0;
    }
  
    int credit = ifCredit(Borrow);
    if (credit == 2) {    
       result["status"] = "failure";
       result["message"] = "Error getting credit";
       std::cout << result.dump() << std::endl;
        return 0;
    }
    if (credit == 0) {
       result["status"] = "failure";
       result["message"] = "error：借书失败！信誉度不足无法借书";
       std::cout << result.dump() << std::endl;
        return 0;
    }

    int limit = ifBelowLimit(Borrow);
    if (limit == 2) {      
       result["status"] = "failure";
       result["message"] = "Error getting borrow limit status";
       std::cout << result.dump() << std::endl;
        return 0;
    }
    if (limit == 0) {
       result["status"] = "failure";
       result["message"] = "error：借书失败！借书量已达上限";
       std::cout << result.dump() << std::endl;
        return 0;
    }
    return 1;
}
//向借阅记录表中加项，添加借书信息
int AddBorrowList(const nlohmann::json Borrow) {//成功返回1，失败0

    std::string ISBN = extractString(Borrow, "ISBN");
    std::string callNum = extractString(Borrow, "callNum");
    std::string bookTitle = extractString(Borrow, "bookTitle");
    std::string uID = extractString(Borrow, "uID");
    std::string userName = extractString(Borrow, "userName");
    std::string borrowDate = extractString(Borrow, "borrowDate");
    std::string dueDate = extractString(Borrow, "dueDate");

    nlohmann::json result;
   
    if (ISBN.empty() || callNum.empty() || bookTitle.empty() || uID.empty() || userName.empty() || borrowDate.empty() || dueDate.empty()) {        
       result["status"] = "failure";
       result["message"] = "Missing required fields in JSON!";
       std::cout << result.dump() << std::endl;
        return 0;
    }
    int valid = 1;
    
    char sql[SQL_MAX];    
    std::snprintf(sql, SQL_MAX, "INSERT INTO borrow_list ( ISBN,callNum,bookTitle,uID,userName,borrowDate,dueDate,valid ) VALUES('%s', '%s','%s', '%s','%s', '%s','%s', %d);", ISBN.c_str(), callNum.c_str(), bookTitle.c_str(), uID.c_str(), userName.c_str(), borrowDate.c_str(), dueDate.c_str(), valid);//.c_str()将string转换为const char*类型
    //std::cout << "Insert statement: " << sql << std::endl;    

    int ifsuccess = mysql_real_query(conn, sql,(unsigned long) strlen(sql));
    if (ifsuccess == 1) {      
        const char* error_msg = mysql_error(conn);
        std::cerr << "MySQL Error: " << error_msg << std::endl;        
       result["status"] = "failure";
       result["message"] = "Add to BorrowList Failed: " + std::string(error_msg);
       std::cout << result.dump() << std::endl;
        return 0;
    }
    return 1;
}

//流通库表中的在馆数量减一
int DecreaseLeftNum(const nlohmann::json Borrow) {//成功返回1，失败返回0

    std::string ISBN = extractString(Borrow, "ISBN");
    int left = GetLeftNum(Borrow);
    left--;
    char sql[SQL_MAX];
    std::snprintf(sql, SQL_MAX, "UPDATE book_circulation SET leftNum= %d WHERE ISBN ='%s';", left,ISBN.c_str());
    // std::cout << "UPDATE statement: " << sql << std::endl;

    nlohmann::json result;
    int ifsuccess = mysql_real_query(conn, sql, (unsigned long)strlen(sql));
    if (ifsuccess == 1) {
        const char* error_msg = mysql_error(conn);
        std::cerr << "MySQL Error: " << error_msg << std::endl;
        result["status"] = "failure";
        result["message"] = "Add to BorrowList Failed: " + std::string(error_msg);
        std::cout << result.dump() << std::endl;
        return 0;
    }
    return 1;
}
 
void BorrowBook(const nlohmann::json Borrow){ //借书
    
    nlohmann::json result;
    int authority = BorrowAuthority(Borrow);
    if (authority == 0) {        
        return ;
    }
    else if (authority == 1) {
        if (AddBorrowList(Borrow) == 0) {
            return;
        }
        if (DecreaseLeftNum(Borrow) == 0) {
            return;
        }
        result["status"] = "success";
        result["message"] = "Borrow book success";
        std::cout << result.dump() << std::endl;
       
        return;
    }
}


//-------------------------------------------------------还书-----------------------------------------------------------

void ReturnBook(const nlohmann::json Return) {

    nlohmann::json result;
    int over = ifOverDue(Return);
    if (over == -1) {
        return;
    }
    else if (over == 1) {
        //使用用户子系统提供的方法将用户信任位置0；
    }
    if (DelBorrowList(Return) == 0) {
        return;
    }
    if (IncreaseLeftNum(Return) == 0) {
        return;
    }
    result["status"] = "success";
    result["message"] = "Reserve book success";
    std::cout << result.dump() << std::endl;
    // cout << "Success";
    return;
}
int ifOverDue(const nlohmann::json Return) {
    int over = 0;                //  string转int
    nlohmann::json result;
    if (Return.contains("ifOver")) {
        std::string strover = Return["ifOver"].get<std::string>();
        if (strover.empty()) {
            result["status"] = "failure";
            result["message"] = "Missing required fields in JSON!";
            std::cout << result.dump() << std::endl;
            std::cout << "Missing required fields(ifOver) in JSON!" << std::endl;
            return -1;
        }
        else {
            over = std::stoi(strover);
        }        
    }
    if (over == 1) {
        return 1;
    }
    return 0;
}
int DelBorrowList(const nlohmann::json Return) {

    std::string ISBN = extractString(Return, "ISBN");   
    std::string uID = extractString(Return, "uID");
    std::string borrowDate = extractString(Return, "borrowDate");

    nlohmann::json result;

    if (ISBN.empty() || uID.empty() || borrowDate.empty()) {
        result["status"] = "failure";
        result["message"] = "Missing required fields in JSON!";
       std::cout << result.dump() << std::endl;
       std::cout << "Missing required fields(ISBN) in JSON!" << std::endl;
        return 0;
    }
    char sql[SQL_MAX];
   
    std::snprintf(sql, sizeof(sql),"UPDATE borrow_list SET valid = 0 WHERE (ISBN = '%s' AND uID = '%s' AND valid = 1)",ISBN.c_str(), uID.c_str());
    // std::cout << "Update statement: " << sql << std::endl;

    // 执行UPDATE语句
    int ifsuccess = mysql_real_query(conn, sql, (unsigned long)strlen(sql));
    if (ifsuccess==1) {
        const char* error_msg = mysql_error(conn);
        
        result["status"] = "failure";
        result["message"] = "Delete BorrowList Failed: " + std::string(error_msg);
        std::cout << result.dump() << std::endl;
        std::cout << "Failed更新借阅记录 :" << std::string(error_msg) << std::endl;
        return 0;
    }
    return 1;
}
int IncreaseLeftNum(const nlohmann::json Return) {

    std::string ISBN = extractString(Return, "ISBN");

    int leftNum = GetLeftNum(Return);
    leftNum++;
    char sql[SQL_MAX];
    std::snprintf(sql, sizeof(sql), "UPDATE book_circulation SET leftNum = %d WHERE ISBN = '%s' ", leftNum, ISBN.c_str());
    // std::cout << "Update statement: " << sql << std::endl;

    nlohmann::json result;
    // 执行UPDATE语句
    if (mysql_real_query(conn, sql, (unsigned long)strlen(sql)) != 0) {
        const char* error_msg = mysql_error(conn);        
        result["status"] = "failure";
        result["message"] = "Increase Left Number Failed: " + std::string(error_msg);
        std::cout << result.dump() << std::endl;
        std::cout << "Increase Left Number Failed:" << std::string(error_msg)<< std::endl;
        return 0;
    }
    return 1;
}