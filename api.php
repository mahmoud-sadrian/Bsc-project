<?php
// فعال کردن error reporting
error_reporting(E_ALL);
ini_set('display_errors', 1);

// حل مشکل CORS
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type, Authorization");
header("Content-Type: application/json");

if ($_SERVER['REQUEST_METHOD'] == 'OPTIONS') {
    exit(0);
}

// Config
define('DB_HOST', 'localhost');
define('DB_USER', 'root');
define('DB_PASS', '');
define('DB_NAME', 'smartify24');

// Start session با تنظیمات امنیتی
session_start([
    'cookie_httponly' => true,
    'cookie_secure' => false,
    'use_strict_mode' => true
]);

// تابع اتصال به دیتابیس
function getDb() {
    static $db = null;
    if ($db === null) {
        try {
            $db = new PDO("mysql:host=" . DB_HOST . ";dbname=" . DB_NAME . ";charset=utf8mb4", DB_USER, DB_PASS);
            $db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
            $db->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
        } catch (PDOException $e) {
            sendJson(['error' => 'Database connection failed: ' . $e->getMessage()], 500);
        }
    }
    return $db;
}

// تابع ارسال پاسخ JSON
function sendJson($data, $status = 200) {
    http_response_code($status);
    echo json_encode($data, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT);
    exit;
}

// تابع احراز هویت
function authenticate() {
    if (!isset($_SESSION['user_id'])) {
        sendJson(['error' => 'Authentication required. Please login first.'], 401);
    }
    return $_SESSION['user_id'];
}

// تابع اعتبارسنجی دستگاه (بررسی مالکیت)
function validateDeviceOwnership($deviceId, $userId) {
    $db = getDb();
    $stmt = $db->prepare("SELECT id FROM devices WHERE id = ? AND user_id = ?");
    $stmt->execute([$deviceId, $userId]);
    return $stmt->fetch() !== false;
}

// تابع لاگ فعالیت
function logActivity($deviceId, $action) {
    try {
        $db = getDb();
        $stmt = $db->prepare("INSERT INTO activity_logs (device_id, action) VALUES (?, ?)");
        $stmt->execute([$deviceId, $action]);
    } catch (PDOException $e) {
        error_log("Activity log error: " . $e->getMessage());
    }
}

// دریافت داده‌های ورودی
$method = $_SERVER['REQUEST_METHOD'];
$input = json_decode(file_get_contents('php://input'), true) ?: [];
$action = $_GET['action'] ?? '';
$deviceId = $_GET['device_id'] ?? null;
$subAction = $_GET['sub_action'] ?? '';

// صفحه اصلی API
if ($method === 'GET' && empty($action)) {
    sendJson([
        'message' => 'Smartify24 API 🚀',
        'version' => '1.0',
        'timestamp' => date('Y-m-d H:i:s'),
        'endpoints' => [
            'Authentication' => [
                'POST /api.php?action=signup' => 'Register new user',
                'POST /api.php?action=signin' => 'User login',
                'POST /api.php?action=logout' => 'User logout'
            ],
            'Devices' => [
                'GET /api.php?action=devices' => 'Get user devices',
                'POST /api.php?action=devices&sub_action=control&device_id=1' => 'Control device',
                'POST /api.php?action=devices&sub_action=timer&device_id=1' => 'Set timer',
                'POST /api.php?action=devices&sub_action=schedule&device_id=1' => 'Set schedule',
                'GET /api.php?action=devices&sub_action=logs&device_id=1' => 'Get device logs',
                'GET /api.php?action=devices&sub_action=status&device_id=1' => 'Get device status'
            ]
        ]
    ]);
}

// ثبت نام کاربر
if ($method === 'POST' && $action === 'signup') {
    if (!isset($input['username'], $input['password'], $input['agreed_to_terms'])) {
        sendJson(['error' => 'Username, password, and terms agreement are required'], 400);
    }
    
    if (!$input['agreed_to_terms']) {
        sendJson(['error' => 'You must agree to the terms and conditions'], 400);
    }
    
    if (strlen($input['username']) < 3) {
        sendJson(['error' => 'Username must be at least 3 characters long'], 400);
    }
    
    if (strlen($input['password']) < 6) {
        sendJson(['error' => 'Password must be at least 6 characters long'], 400);
    }
    
    $first_name = trim($input['first_name'] ?? '');
    $last_name = trim($input['last_name'] ?? '');
    $hash = password_hash($input['password'], PASSWORD_BCRYPT);
    
    $db = getDb();
    try {
        // بررسی وجود username
        $checkStmt = $db->prepare("SELECT id FROM users WHERE username = ?");
        $checkStmt->execute([$input['username']]);
        if ($checkStmt->fetch()) {
            sendJson(['error' => 'Username already exists'], 409);
        }
        
        $stmt = $db->prepare("INSERT INTO users (first_name, last_name, username, password_hash, agreed_to_terms) VALUES (?, ?, ?, ?, ?)");
        $stmt->execute([$first_name, $last_name, $input['username'], $hash, 1]);
        
        sendJson([
            'message' => 'User registered successfully',
            'userId' => $db->lastInsertId(),
            'username' => $input['username']
        ], 201);
        
    } catch (PDOException $e) {
        sendJson(['error' => 'Registration failed: ' . $e->getMessage()], 500);
    }
}

// ورود کاربر
if ($method === 'POST' && $action === 'signin') {
    if (!isset($input['username'], $input['password'])) {
        sendJson(['error' => 'Username and password are required'], 400);
    }
    
    $db = getDb();
    $stmt = $db->prepare("SELECT * FROM users WHERE username = ?");
    $stmt->execute([$input['username']]);
    $user = $stmt->fetch();
    
    if (!$user || !password_verify($input['password'], $user['password_hash'])) {
        sendJson(['error' => 'Invalid username or password'], 401);
    }
    
    $_SESSION['user_id'] = $user['id'];
    $_SESSION['username'] = $user['username'];
    $_SESSION['login_time'] = time();
    
    sendJson([
        'message' => 'Login successful',
        'userId' => $user['id'],
        'username' => $user['username'],
        'firstName' => $user['first_name'],
        'lastName' => $user['last_name']
    ]);
}

// خروج کاربر
if ($method === 'POST' && $action === 'logout') {
    session_destroy();
    sendJson(['message' => 'Logout successful']);
}

// مدیریت دستگاه‌ها
if ($action === 'devices') {
    $userId = authenticate();
    
    // دریافت لیست دستگاه‌ها
    if ($method === 'GET' && empty($subAction)) {
        $db = getDb();
        $stmt = $db->prepare("SELECT * FROM devices WHERE user_id = ? ORDER BY name");
        $stmt->execute([$userId]);
        $devices = $stmt->fetchAll();
        
        sendJson([
            'devices' => $devices,
            'count' => count($devices)
        ]);
    }
    
    // کنترل دستگاه
    if ($method === 'POST' && $subAction === 'control' && $deviceId) {
        if (!isset($input['status']) || !in_array($input['status'], ['ON', 'OFF'])) {
            sendJson(['error' => 'Valid status (ON or OFF) is required'], 400);
        }
        
        if (!validateDeviceOwnership($deviceId, $userId)) {
            sendJson(['error' => 'Device not found or access denied'], 404);
        }
        
        $db = getDb();
        $stmt = $db->prepare("UPDATE devices SET status = ?, last_updated = NOW() WHERE id = ?");
        $stmt->execute([$input['status'], $deviceId]);
        
        logActivity($deviceId, "Manually turned " . $input['status']);
        
        sendJson([
            'message' => 'Device status updated successfully',
            'deviceId' => $deviceId,
            'status' => $input['status']
        ]);
    }
    
    // تنظیم تایمر
    if ($method === 'POST' && $subAction === 'timer' && $deviceId) {
        if (!isset($input['duration_minutes']) || !is_numeric($input['duration_minutes']) || $input['duration_minutes'] <= 0) {
            sendJson(['error' => 'Valid duration in minutes is required'], 400);
        }
        
        if (!validateDeviceOwnership($deviceId, $userId)) {
            sendJson(['error' => 'Device not found or access denied'], 404);
        }
        
        $db = getDb();
        
        // غیرفعال کردن تایمرهای قبلی
        $stmt = $db->prepare("UPDATE schedules SET active = 0 WHERE device_id = ? AND type = 'TIMER'");
        $stmt->execute([$deviceId]);
        
        // ایجاد تایمر جدید
        $stmt = $db->prepare("INSERT INTO schedules (device_id, type, duration_minutes, active, created_at) VALUES (?, 'TIMER', ?, 1, NOW())");
        $stmt->execute([$deviceId, intval($input['duration_minutes'])]);
        
        // روشن کردن دستگاه
        $stmt = $db->prepare("UPDATE devices SET status = 'ON', last_updated = NOW() WHERE id = ?");
        $stmt->execute([$deviceId]);
        
        logActivity($deviceId, "Timer set for " . $input['duration_minutes'] . " minutes");
        
        sendJson([
            'message' => 'Timer set successfully',
            'deviceId' => $deviceId,
            'duration' => $input['duration_minutes'],
            'status' => 'ON'
        ]);
    }
    
    // دریافت لاگ‌های دستگاه
    if ($method === 'GET' && $subAction === 'logs' && $deviceId) {
        if (!validateDeviceOwnership($deviceId, $userId)) {
            sendJson(['error' => 'Device not found or access denied'], 404);
        }
        
        $db = getDb();
        $stmt = $db->prepare("SELECT * FROM activity_logs WHERE device_id = ? ORDER BY timestamp DESC LIMIT 50");
        $stmt->execute([$deviceId]);
        $logs = $stmt->fetchAll();
        
        sendJson([
            'deviceId' => $deviceId,
            'logs' => $logs,
            'count' => count($logs)
        ]);
    }
    
    // دریافت وضعیت دستگاه
    if ($method === 'GET' && $subAction === 'status' && $deviceId) {
        $db = getDb();
        
        // بررسی مالکیت (اختیاری برای ESP)
        $stmt = $db->prepare("SELECT status FROM devices WHERE id = ?");
        $stmt->execute([$deviceId]);
        $device = $stmt->fetch();
        
        if (!$device) {
            sendJson(['error' => 'Device not found'], 404);
        }
        
        sendJson([
            'deviceId' => $deviceId,
            'status' => $device['status'],
            'timestamp' => date('Y-m-d H:i:s')
        ]);
    }
}

// اگر action شناخته شده نبود
sendJson(['error' => 'Invalid action or endpoint not found'], 404);
?>