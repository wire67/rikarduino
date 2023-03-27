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

if ($config_ar['alertmailepoch'] + 60 * 60 * 12 < time() // check last email was more than 12H ago.
 && issetOr($config_ar['noalertepoch'], 0) + 60 * 60 * 12 > time() // check if this alert is new, less than 12H ago it wasn't alert.
   ) {
   if (isAlert($decoded)) {
      $headers = array(
         'CC' => CREDENTIAL_EMAIL_REPLYTO,
         'Reply-To' => CREDENTIAL_EMAIL_REPLYTO,
      );
      mail(CREDENTIAL_EMAIL_DEST, 'Automatique: Pellet épuisé?', "Vérifier niveau de pellet\r\nStatus: http://rika.yupanaengineering.com/gite", $headers);
      $config_ar['alertmailepoch'] = time();
      $jsonConfigStr = json_encode($config_ar, JSON_PRETTY_PRINT);
      file_put_contents($configFilePath, $jsonConfigStr);
   } else {
      $config_ar['noalertepoch'] = time();
      $jsonConfigStr = json_encode($config_ar, JSON_PRETTY_PRINT);
      file_put_contents($configFilePath, $jsonConfigStr);
   }
}
