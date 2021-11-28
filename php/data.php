<?php
$configFilePath = "config.json";
$statusFilePath = "status.json";

require_once('lib.php');
header('Content-Type: application/json; charset=utf-8');
//Get data from existing json file
$jsonConfigStr = file_get_contents($configFilePath);
// converts json data into array
$config_ar = json_decode($jsonConfigStr, true);

if (ismode(0)) {
   // --- CALENDAR ---
   processCalendar($config_ar);
}

$config_ar['timestamp'] = time();
// --- JSON ---
echo json_encode($config_ar);

/* ===================================================== */
// --- FINISH REQUEST --- //
fastcgi_finish_request(); // Fast-Fpm close connection, can't send text after this point.

// --- POST ---
$status = json_decode(file_get_contents($statusFilePath), true);
$jsonStatusStr = file_get_contents("php://input");
$decoded = json_decode($jsonStatusStr, true);
$decoded['timestamp'] = time();

// check data integrity
if ($decoded['nowtemp'] > 1) {
   $status[$decoded['mac']] = $decoded;
   file_put_contents($statusFilePath, json_encode($status));
}

// --- MONGO ---
require_once('MyMongo.php');
MyMongo::PushStatus($decoded);

if ($config_ar['alertmailepoch'] + 60 * 60 * 12 < time()) {
   if (isAlert($decoded)) {
      mail(CREDENTIAL_EMAIL, 'Pellet épuisé?', 'Vérifier niveau de pellet');
      $config_ar['alertmailepoch'] = time();
      $jsonConfigStr = json_encode($config_ar, JSON_PRETTY_PRINT);
      file_put_contents($configFilePath, $jsonConfigStr);
   }
}
