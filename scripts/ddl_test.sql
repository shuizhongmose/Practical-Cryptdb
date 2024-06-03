-- 创建数据库
CREATE DATABASE IF NOT EXISTS encrypted_db;
USE encrypted_db;

-- 创建包含各种数据类型的表
CREATE TABLE employees (
    employee_id INT AUTO_INCREMENT PRIMARY KEY,
    first_name VARCHAR(50) NOT NULL,
    last_name VARCHAR(50) NOT NULL,
    email VARCHAR(100),
    salary DECIMAL(10,2),
    start_date DATE,
    bio TEXT,
    picture BLOB
);

-- 添加新列
ALTER TABLE employees ADD COLUMN status VARCHAR(10) DEFAULT 'active';

-- 修改列属性，不支持
ALTER TABLE employees MODIFY COLUMN salary DECIMAL(12,2);

-- 创建索引
CREATE INDEX idx_salary ON employees(salary);

-- 删除列
ALTER TABLE employees DROP COLUMN picture;

-- 删除索引
DROP INDEX idx_salary ON employees;

-- 删除表
DROP TABLE employees;

-- 创建视图，不支持
CREATE VIEW active_employees AS
SELECT employee_id, first_name, last_name, email
FROM employees
WHERE status = 'active';

-- 修改视图，不支持
CREATE OR REPLACE VIEW active_employees AS
SELECT employee_id, first_name, last_name, email, start_date
FROM employees
WHERE status = 'active';

-- 删除视图
DROP VIEW active_employees;

-- 删除数据库
DROP DATABASE encrypted_db;
