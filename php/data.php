<?php
$configFilePath = "config.json";
$statusFilePath = "status.json";

require_once('credentials.php');
require_once('lib.php');
header('Content-Type: application/json; charset=utf-8');
//Get data from existing json file
$jsonConfigStr = file_get_contents($configFilePath);
// converts json data into array
$config_ar = json_decode($jsonConfigStr, true);

if (isMode(0)) {
   // --- CALENDAR ---
   processCalendar($config_ar); // /!\ modifies $config_ar to extract settemp based on nowtime
}


// --- POST ---
$statusAll = json_decode(file_get_contents($statusFilePath), true);
$jsonStatusStr = file_get_contents("php://input");
$statusSingle = json_decode($jsonStatusStr, true);
$statusSingle['timestamp'] = time(); // record last time online

if (isMode(6) && 'Power'==$statusSingle['swbtnlast'])
{
   // if switched on in the last 1mn and 'manual'==6 then move to 'manual'=5
   if(  $statusSingle['swbtnpress']     < $statusSingle['rikastart']
    && ($statusSingle['swbtnpress']+60) > $statusSingle['rikastart'] )
   {
      // converts json data into array
      $config_ar[CFG_ROOM_NAME]['manual'] = 5;
      $jsonConfigStr = json_encode($config_ar, JSON_PRETTY_PRINT);
      file_put_contents($configFilePath, $jsonConfigStr);
   }
}

// --- JSON ---
$config_ar['timestamp'] = time(); // pass-on the current timestamp to FW for rtc.setTime()
echo json_encode($config_ar);

/* ===================================================== */
// --- FINISH REQUEST --- //
fastcgi_finish_request(); // Fast-Fpm close connection, can't send text after this point.

// check data integrity
if ($statusSingle['nowtemp'] > 1) {
   $statusAll[$statusSingle['mac']] = $statusSingle;
   file_put_contents($statusFilePath, json_encode($statusAll));
}

// --- MONGO ---
require_once('MyMongo.php');
MyMongo::PushStatus($statusSingle);

if ($config_ar['alertmailepoch'] + 60 * 60 * 12 < time() // check last email was more than 12H ago.
 && issetOr($config_ar['noalertepoch'], 0) + 60 * 60 * 12 > time() // check if this alert is new, less than 12H ago it wasn't alert.
   ) {
   if (isAlert($statusSingle)) {
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
