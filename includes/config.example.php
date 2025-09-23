<?php

define('DB_HOST', 'localhost');
define('DB_USER', 'your_db_username');
define('DB_PASS', 'your_db_password');
define('DB_NAME', 'your_database_name');

if (basename(__FILE__) === basename($_SERVER['SCRIPT_FILENAME'])) {
    header('HTTP/1.0 403 Forbidden');
    exit('Forbidden Access');
}


// This is just a sample file
// Change the values after cloning