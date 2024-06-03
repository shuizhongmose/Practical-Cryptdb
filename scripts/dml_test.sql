-- 使用测试数据库
CREATE DATABASE IF NOT EXISTS encrypted_db;
USE encrypted_db;

-- 创建部门表
CREATE TABLE departments (
    department_id INT AUTO_INCREMENT PRIMARY KEY,
    department_name VARCHAR(50),
    budget DECIMAL(10, 2)
);

-- 插入部门数据
INSERT INTO departments (department_name, budget) VALUES
('HR', 50000),
('Development', 150000),
('Sales', 75000);

-- 创建员工表，FOREIGN KEY不支持，因为加密之后ID更高
CREATE TABLE employees (
    employee_id INT AUTO_INCREMENT PRIMARY KEY,
    first_name VARCHAR(50) NOT NULL,
    last_name VARCHAR(50) NOT NULL,
    email VARCHAR(100),
    salary DECIMAL(10,2),
    start_date DATE,
    department_id INT,
    bio TEXT
    -- FOREIGN KEY (department_id) REFERENCES departments(department_id)
);

-- 插入员工数据
INSERT INTO employees (first_name, last_name, email, salary, start_date, department_id, bio) VALUES
('John', 'Doe', 'john.doe@example.com', 70000, '2020-01-10', 1, 'Experienced manager in HR'),
('Jane', 'Smith', 'jane.smith@example.com', 85000, '2020-05-15', 2, 'Senior developer from Dev team'),
('Jim', 'Beam', 'jim.beam@example.com', 65000, '2020-06-10', 1, 'Sales representative with experience'),
('Jack', 'White', 'jack.white@example.com', 50000, '2020-02-20', 3, 'Sales intern, eager to learn'),
('Jill', 'Stark', 'jill.stark@example.com', 90000, '2020-03-15', 2, 'Lead architect in Development');

-- 查询测试
---- 仅选择员工的薪水和部门ID，使用基本的WHERE条件
SELECT salary, department_id
FROM employees
WHERE salary > 60000;

---- 选择所有部门的记录
SELECT department_id, department_name
FROM departments;

-- 联合操作，关联部门表，选择员工的薪水和部门名称
SELECT e.employee_id, e.salary, d.department_name
FROM employees as e, departments as d
where e.salary > 60000
GROUP BY e.employee_id;

---- 使用聚合函数AVG计算符合条件的员工的平均薪水
SELECT AVG(e.salary) AS average_salary
FROM employees as e, departments as d
where e.salary > 60000
GROUP BY d.department_name;

---- 添加GROUP BY，按部门名称分组计算平均薪水
SELECT d.department_name, AVG(e.salary) AS average_salary
FROM employees as e, departments as d
where e.salary > 60000;
GROUP BY d.department_name;

---- 使用DISTINCT和ORDER BY对结果进行去重和排序
SELECT DISTINCT d.department_name, AVG(e.salary) AS average_salary
FROM employees as e, departments as d
WHERE e.salary > 60000
GROUP BY d.department_name
ORDER BY average_salary DESC;

---- 复杂测试，【错误，不支持JOIN】
SELECT DISTINCT d.department_name, AVG(e.salary) AS average_salary
FROM employees e
JOIN departments d ON e.department_id = d.department_id
WHERE e.salary > 60000
GROUP BY d.department_name
ORDER BY average_salary DESC;

-- 使用WITH ROLLUP
SELECT department_name, SUM(budget) AS total_budget
FROM departments
GROUP BY department_name WITH ROLLUP;

-- 使用LIKE操作，【错误，不支持LIKE】
SELECT * FROM employees
WHERE bio LIKE '%experience%';

-- 使用IN操作
SELECT first_name, last_name, department_id
FROM employees
WHERE department_id IN (1, 2);

-- 使用BETWEEN操作
SELECT first_name, last_name, salary
FROM employees
WHERE salary BETWEEN 60000 AND 90000;

-- 使用CASE语句
SELECT first_name, last_name,
CASE
    WHEN salary > 80000 THEN 'High salary'
    WHEN salary BETWEEN 60000 AND 80000 THEN 'Average salary'
    ELSE 'Low salary'
END AS salary_level
FROM employees;

-- 使用HAVING过滤聚合结果，【错误】
SELECT department_name, COUNT(*) AS num_employees, AVG(salary) AS avg_salary
FROM employees as e, departments as d
GROUP BY department_name
HAVING avg_salary > 65000;

-- 使用LIMIT获取前3个高薪员工
SELECT first_name, last_name, salary
FROM employees
ORDER BY salary DESC
LIMIT 3;

-- 多条件查询和复杂的JOIN操作，【错误】
SELECT e.first_name, e.last_name, d.department_name, e.salary
FROM employees e
LEFT JOIN departments d ON e.department_id = d.department_id
WHERE (e.salary BETWEEN 50000 AND 90000) AND d.department_name IN ('HR', 'Development')
ORDER BY e.salary;

-- 清理测试环境
DROP TABLE IF EXISTS employees;
DROP TABLE IF EXISTS departments;
