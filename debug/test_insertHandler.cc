#include <iostream>
#include <vector>
#include <functional>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <main/Connect.hh>
#include <main/rewrite_util.hh>
#include <main/sql_handler.hh>
#include <main/dml_handler.hh>
#include <main/ddl_handler.hh>
#include <main/CryptoHandlers.hh>

static std::string embeddedDir="/t/cryt/shadow";
/*
//Show what Analysis can do 
static void test_Analysis(Analysis & analysis){
    //oDET,
    //oOPE,
    //oAGG,
    std::cout<<GREEN_BEGIN<<"USE Analysis: "<<COLOR_END<<std::endl;
    OnionMeta *om = analysis.getOnionMeta2("tdb","student","id",oDET);
    if(om!=NULL)
        std::cout<<"use analysis.getOnionMetae oDET: "<<om->getAnonOnionName()<<std::endl; 
    om = analysis.getOnionMeta2("tdb","student","id",oOPE);
    if(om!=NULL)
        std::cout<<"use analysis.getOnionMetae oOPE: "<<om->getAnonOnionName()<<std::endl; 

    om = analysis.getOnionMeta2("tdb","student","id",oAGG);
    if(om!=NULL)
        std::cout<<"use analysis.getOnionMetae oAGG: "<<om->getAnonOnionName()<<std::endl;
}*/

static void myTestCreateTableHandler(std::string query){
    std::unique_ptr<Connect> e_conn(Connect::getEmbedded(embeddedDir));
    std::unique_ptr<SchemaInfo> schema(new SchemaInfo());
    std::function<DBMeta *(DBMeta *const)> loadChildren =
        [&loadChildren, &e_conn](DBMeta *const parent) {
            auto kids = parent->fetchChildren(e_conn);
            for (auto it : kids) {
                loadChildren(it);
            }
            return parent;
        };
    //load all metadata and then store it in schema
    loadChildren(schema.get()); 
    const std::unique_ptr<AES_KEY> &TK = std::unique_ptr<AES_KEY>(getKey(std::string("113341234")));
    //just like what we do in Rewrite::rewrite,dispatchOnLex
    Analysis analysis(std::string("sbtest"),*schema,TK,
                        SECURITY_RATING::SENSITIVE);
    //assert(analysis.getMasterKey().get()!=NULL);
    //assert(getKey(std::string("113341234"))!=NULL);
    //test_Analysis(analysis);

    DMLHandler *h = new InsertHandler();

    std::unique_ptr<query_parse> p;
    p = std::unique_ptr<query_parse>(
                new query_parse("sbtest", query));
    LEX *const lex = p->lex();
    auto executor = h->transformLex(analysis,lex);
    // std::cout<<  ((DMLQueryExecutor*)executor)->getQuery()<<std::endl;

}

int
main() {
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }
    embeddedDir = std::string(buffer)+"/shadow";
    const std::string master_key = "113341234";
    ConnectionInfo ci("localhost", "root", "letmein", 3306);
    SharedProxyState *shared_ps = new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    assert(shared_ps!=NULL);
    std::string query1 = "INSERT INTO user (name, age) VALUES ('alice', 19), ('bob', 20), ('chris', 21);";
    std::vector<std::string> querys{query1};
    for(auto item:querys){
        std::cout<<item<<std::endl;
        myTestCreateTableHandler(item);
        std::cout<<std::endl;
    }
    return 0;
}
