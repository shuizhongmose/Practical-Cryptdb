-- 创建数据库
CREATE DATABASE encrypted_db;
USE encrypted_db;

-- 创建表
CREATE TABLE Employees (
    EmployeeID INT PRIMARY KEY,
    FirstName VARCHAR(50),
    LastName VARCHAR(50),
    DepartmentID INT,
    Salary DECIMAL(10, 2)
);

CREATE TABLE Departments (
    DepartmentID INT PRIMARY KEY,
    DepartmentName VARCHAR(50)
);

CREATE TABLE Projects (
    ProjectID INT PRIMARY KEY,
    ProjectName VARCHAR(50),
    Budget DECIMAL(10, 2)
);

CREATE TABLE EmployeeProjects (
    EmployeeID INT,
    ProjectID INT,
    HoursWorked INT,
    PRIMARY KEY (EmployeeID, ProjectID)
    -- FOREIGN KEY (EmployeeID) REFERENCES Employees(EmployeeID), 【不支持】
    -- FOREIGN KEY (ProjectID) REFERENCES Projects(ProjectID)
);

-- 插入数据
INSERT INTO Employees VALUES
(1, 'John', 'Doe', 1, 50000),
(2, 'Jane', 'Smith', 2, 60000),
(3, 'Jim', 'Brown', 1, 55000);

INSERT INTO Departments VALUES
(1, 'HR'),
(2, 'Engineering');

INSERT INTO Projects VALUES
(1, 'Project A', 100000),
(2, 'Project B', 150000);

INSERT INTO EmployeeProjects VALUES
(1, 1, 100),
(2, 1, 150),
(3, 2, 200);

-- 查询员工信息
SELECT * FROM Employees;

-- 等值连接查询员工和部门信息
SELECT e.EmployeeID, e.FirstName, e.LastName, d.DepartmentName
FROM Employees e
JOIN Departments d ON e.DepartmentID = d.DepartmentID;

-- 使用谓词查询薪水大于55000的员工
SELECT * FROM Employees
WHERE Salary > 55000;

-- 使用GROUP BY和COUNT统计每个部门的员工数量
SELECT DepartmentID, COUNT(*) as EmployeeCount
FROM Employees
GROUP BY DepartmentID;

-- 使用DISTINCT查询不同的部门ID
SELECT DISTINCT DepartmentID
FROM Employees;

-- 使用ORDER BY按薪水降序排序员工
SELECT * FROM Employees
ORDER BY Salary DESC;

-- 使用MIN和MAX查询最低和最高薪水
SELECT MIN(Salary) as MinSalary, MAX(Salary) as MaxSalary
FROM Employees;

-- 使用SUM计算所有项目的总预算
SELECT SUM(Budget) as TotalBudget
FROM Projects;

-- 添加新员工
INSERT INTO Employees (EmployeeID, FirstName, LastName, DepartmentID, Salary)
VALUES (4, 'Alice', 'Johnson', 2, 70000);

-- 更新员工薪水
UPDATE Employees
SET Salary = Salary + 5000
WHERE EmployeeID = 1;

-- 删除一个项目
DELETE FROM Projects
WHERE ProjectID = 2;

-- 查看更新后的数据
SELECT * FROM Employees;
SELECT * FROM Projects;

-- 添加一个新项目并分配员工
INSERT INTO Projects (ProjectID, ProjectName, Budget)
VALUES (3, 'Project C', 200000);

INSERT INTO EmployeeProjects (EmployeeID, ProjectID, HoursWorked)
VALUES (1, 3, 50);

-- 查询员工和项目分配情况
SELECT e.FirstName, e.LastName, p.ProjectName, ep.HoursWorked
FROM EmployeeProjects ep
JOIN Employees e ON ep.EmployeeID = e.EmployeeID
JOIN Projects p ON ep.ProjectID = p.ProjectID;
