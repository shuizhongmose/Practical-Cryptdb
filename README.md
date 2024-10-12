# Make cryptdb Practical

Cryptdb originated from MIT. This is a modified version. In this project, we try to add new features, fix bugs we meet in our environment, and rewrite the code and comments to make the source code easy to understand. Introduction to the features will be posted at yiwenshao.github.io. Also, analysis of the source code will be posted there so that you do not need so much effort contributing to this project.

To deploy this version, you need follow the following steps.

+ have ubuntu 16.04 installed
+ install mysql-server 5.5 or higher, with the root password 'letmein'
	To verify this, you can use the command mysql -uroot -pletmein to log in
+ ./INSTALL.sh
+ source ~/.bashrc
+ source setup.sh
+ run ./cdbserver.sh, if got error, delete shadow directory and restart cdbserver.
+ run ./cdbclient.sh 
+ enjoy it!



If you meet any problems installing it, or if you meet bugs or need new features that if not yet supported, post issues or contact me via shaoyiwenetATgmailDotcom.



New features added

+ foreign key constraint

```
create table student (id integer primary key) ENGINE=InnoDB;
create table choose (sid integer, foreign key fk(sid) references student(id)) ENGINE=InnoDB;
insert into student values(1);
insert into choose values(1);

```

+ set user variable
+ timestamp
+ show create table
+ cdb_test for simple curd

obselete functions deleted

+ annotation
+ dbobject.tt

# features to be added

+ extended-insert
+ QUOTE
+ Search

# Run in Docker 

- build image

  ```
  docker build -it username/cryptdb:0.1 .
  ```

  

- run CDB service container
  - edit `docker/testcdb-compose.yaml`，change `image`
  - run container: `docker-compose -f docker/testcdb-compose.yaml up -d`
  - go into container `cdbservice`, and run `./cdbclient.sh` to start cdb client. Now you can test CDB.
  - copy `docker/cdbcreate.sql` to container, and run it. Now cryptdb created.
  - connect MySQL use port 3306, you can see encrypted  user table.
  
# Run CryptDB benchmark test with SysBench
- [Deployment and Sysbench test Document](./docs/deployment_and_test.md)

# References
> - Article [Bachelor Thesis Analysis of Encrypted Databases with CryptDB](https://www.nds.rub.de/media/ei/arbeiten/2015/10/26/thesis.pdf)
> 
> 
> 【OPE相关论文：】
> - Popa R A, Li F H, Zeldovich N. [An ideal-security protocol for order-preserving encoding](https://eprint.iacr.org/2013/129.pdf)[C]//2013 IEEE Symposium on Security and Privacy. IEEE, 2013: 463-477.
> 
> - Boldyreva A, Chenette N, Lee Y, et al. [Order-preserving symmetric encryption](https://eprint.iacr.org/2012/624.pdf)[C]//Advances in Cryptology-EUROCRYPT 2009: 28th Annual International Conference on the Theory and Applications of Cryptographic Techniques, Cologne, Germany, April 26-30, 2009. Proceedings 28. Springer Berlin Heidelberg, 2009: 224-241.
>
> - Boldyreva A, Chenette N, O’Neill A. [Order-preserving encryption revisited: Improved security analysis and alternative solutions](https://eprint.iacr.org/2012/625.pdf)[C]//Advances in Cryptology–CRYPTO 2011: 31st Annual Cryptology Conference, Santa Barbara, CA, USA, August 14-18, 2011. Proceedings 31. Springer Berlin Heidelberg, 2011: 578-595.
>
> 【格式保持的费斯妥尔加密-FFX 相关论文】
> - Bellare M, Rogaway P, Spies T. [The FFX mode of operation for format-preserving encryption](https://csrc.nist.gov/CSRC/media/Projects/Block-Cipher-Techniques/documents/BCM/proposed-modes/ffx/ffx-spec.pdf)[J]. NIST submission, 2010, 20(19): 1-18.
>
> - Bellare M, Rogaway P, Spies T. [Addendum to “The FFX Mode of Operation for Format-Preserving Encryption”: A parameter collection for enciphering strings of arbitrary radix and length](https://csrc.nist.gov/csrc/media/projects/block-cipher-techniques/documents/bcm/proposed-modes/ffx/ffx-spec2.pdf)[J]. 2010.