<?php
$configFilePath = "config.json";
$statusFilePath = "status.json";

require_once('credentials.php');
require_once('lib.php');
require_once('level_alert.php');
$status = json_decode(file_get_contents($statusFilePath), true);
try {
   //Get data from existing json file
   $jsonConfigStr = file_get_contents($configFilePath);
   // converts json data into array
   $config_ar = json_decode($jsonConfigStr, true);

   if (isMode(0)) {
      // --- OLD CALENDAR ---
      processCalendar($config_ar);
   }

   if (
      isset($_POST['oldsettemp'])
      && $_POST['oldsettemp'] == $config_ar[CFG_ROOM_NAME]['setpoint']
      && isset($_POST['setpoint'])
   ) {
      //Get form data
      $formdata = [CFG_ROOM_NAME => ['setpoint' => (float)($_POST['setpoint'])]];
      $formdata[CFG_ROOM_NAME]['kp'] = (float)($_POST['kp']);
      $formdata[CFG_ROOM_NAME]['ki'] = (float)($_POST['ki']);
      $formdata[CFG_ROOM_NAME]['kd'] = (float)($_POST['kd']);
      $formdata[CFG_ROOM_NAME]['cal'] = $_POST['cal'];
      $formdata[CFG_ROOM_NAME]['manual'] = (int)$_POST['manual'];
      $formdata[CFG_ROOM_NAME]['frommac'] = $_POST['frommac'];
      $formdata[CFG_ROOM_NAME]['onallowed'] = (int)$_POST['onallowed'];
      $formdata[CFG_ROOM_NAME]['offallowed'] = (int)$_POST['offallowed'];
      $formdata[CFG_ROOM_NAME]['udp_enabled'] = (int)$_POST['udp_enabled'];
      // Push user data to array
      $config_ar = $formdata;
      //Convert updated array to JSON
      $jsonConfigStr = json_encode($config_ar, JSON_PRETTY_PRINT);

      //write json data into data.json file
      if (file_put_contents($configFilePath, $jsonConfigStr)) {
         // echo 'Data successfully saved';
      } else {
         echo "error";
      }
   }
} catch (Exception $e) {
   echo 'Caught exception: ',  $e->getMessage(), "\n";
}

if (isMode(0)) {
   // --- NEW CALENDAR ---
   processCalendar($config_ar);
}

$MAC = $config_ar[CFG_ROOM_NAME]['frommac'];

?>
<!doctype html>
<html>

<head>
   <meta charset="utf-8">
   <meta http-equiv="X-UA-Compatible" content="IE=edge">
   <meta name="viewport" content="width=device-width, initial-scale=1">
   <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js"></script>
   <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/css/bootstrap.min.css" integrity="sha384-Gn5384xqQ1aoWXA+058RXPxPg6fy4IWvTNh0E263XmFcJlSAwiGgFAW/dAiS6JXm" crossorigin="anonymous">
   <script src="https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.12.9/umd/popper.min.js" integrity="sha384-ApNbgh9B+Y1QKtv3Rn7W3mgPxhU9K/ScQsAP7hUibX39j7fakFPskvXusvfa0b4Q" crossorigin="anonymous"></script>
   <script src="https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/js/bootstrap.min.js" integrity="sha384-JZR6Spejh4U02d8jOt6vLEHfe/JQGiRRSQQxSfFWpi1MquVdAyjUar5+76PVCmYl" crossorigin="anonymous"></script>
   <!-- charts -->
   <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.5.1/chart.min.js" integrity="sha512-Wt1bJGtlnMtGP0dqNFH1xlkLBNpEodaiQ8ZN5JLA5wpc1sUlk/O5uuOMNgvzddzkpvZ9GLyYNa8w2s7rqiTk5Q==" crossorigin="anonymous" referrerpolicy="no-referrer"></script>

   <style>
      body {
         margin: 20px;
      }

      .nowtemp {
         padding: 10px;
         border: 1px black;
         border-style: solid;
      }

      .selected {
         padding: 10px;
         border: 5px green;
         border-style: double;
      }

      .notselected {
         background-color: lightgrey;
         padding: 10px;
         border: 2px darkgray;
         border-style: solid;
      }

      label {
         margin-top: 5px;
         padding: 3px;
         border: 1px grey solid;
      }
   </style>
</head>

<body>

   <form action="?" method="POST" name="myform">
      <input type="hidden" value="<?= $config_ar[CFG_ROOM_NAME]['setpoint'] ?>" name="oldsettemp" />
      <div class="nowtemp text-center">
         Thermomètre °C:
         <?= (issetOr($status[$MAC]['timestamp'], 0) + 60 > time()) ?
            '<h2>' . round(issetOr($status[$MAC]['nowtemp'], ''), 1) . '</h2>' :
            '<span class="badge badge-danger" title="' . (new DateTime())->setTimestamp(issetOr($status[$MAC]['timestamp'], 0))->format('Y-m-d\TH:i:s') . '">Hors Ligne</span>'
         ?>
         <?php
         $last_alert = get_last_level_alert($config_ar);
         if (!empty($last_alert)) {
            echo '<span class="badge badge-warning" title="' . $last_alert . '">Pellet épuisé</span>';
         }
         ?>
      </div>
      <div class="text-center mt-2">
         <label class="rounded border border-<?= isMode(1) ? 'primary' : 'secondary' ?>">
            <input type="radio" name="manual" value="1" <?= isMode(1) ? 'checked' : '' ?> onclick="document.myform.submit();" />
            Température Constante
         </label>
         <label class="rounded border border-<?= isMode(0) ? 'primary' : 'secondary' ?>">
            <input type="radio" name="manual" value="0" <?= isMode(0) ? 'checked' : '' ?> onclick="$('#onallowed').attr('checked',true);$('#offallowed').attr('checked',true);document.myform.submit();" />
            Calendrier
         </label>
         <!-- <label class="rounded border border-<?= isMode(2) ? 'primary' : 'secondary' ?>">
            <input type="radio" name="manual" value="2" <?= isMode(2) ? 'checked' : '' ?> onclick="document.myform.submit();" disabled="disabled" />
            Rika Thermostat
         </label> -->
         <label class="rounded border border-<?= isMode(3) ? 'primary' : 'secondary' ?>">
            <input type="radio" name="manual" value="3" <?= isMode(3) ? 'checked' : '' ?> onclick="$('#setpoint').val(12);$('#onallowed').attr('checked',true);$('#offallowed').attr('checked',true);document.myform.submit();" />
            Hors Gel
         </label>
         <!-- <label class="rounded border border-<?= isMode(4) ? 'primary' : 'secondary' ?>">
            <input type="radio" name="manual" value="4" <?= isMode(4) ? 'checked' : '' ?> onclick="document.myform.submit();" disabled="disabled" />
            Rika Standby
         </label> -->
         <label class="rounded border border-<?= isMode(5) ? 'primary' : 'secondary' ?>">
            <input type="radio" name="manual" value="5" <?= isMode(5) ? 'checked' : '' ?> onclick="$('#setpoint').val(12);$('#onallowed').attr('checked',true);$('#offallowed').attr('checked',false);document.myform.submit();" />
            Flamme 0%
         </label>
         <label class="rounded border border-<?= isMode(6) ? 'primary' : 'secondary' ?>">
            <input type="radio" name="manual" value="6" <?= isMode(6) ? 'checked' : '' ?> onclick="$('#onallowed').attr('checked',false);$('#offallowed').attr('checked',true);document.myform.submit();" />
            Off
         </label>
      </div>
      <div id="div_manual" class="<?= isMode(1) ? 'selected' : 'notselected' ?> text-center mt-2">
         Consigne °C:
         <br>
         <input type="button" value="-" onclick="setpoint.value=parseFloat(setpoint.value)-0.5;$('[name=manual][value=1]').attr('checked',1);document.myform.submit();" />
         <input name="setpoint" id="setpoint" type="text" value="<?= $config_ar[CFG_ROOM_NAME]['setpoint'] ?>" pattern="[0-9]{2}(.[0-9]{1,2})?" size="1">
         <input type="button" value="+" onclick="setpoint.value=parseFloat(setpoint.value)+0.5;$('[name=manual][value=1]').attr('checked',1);document.myform. submit();" />
      </div>
      <div id="div_calendar" class="<?= isMode(0) ? 'selected' : 'notselected' ?> text-center mt-2">
         Calendrier:
         <?php
         $i = 0;
         $cal_ar = issetOr($config_ar[CFG_ROOM_NAME]['cal'], []);
         usort($cal_ar, "array_cal_cmp");
         foreach ($cal_ar as $cal) {
            if (!empty($cal['h']) || !empty($cal['temp'])) {
         ?>
               <br>
               après
               <input name="cal[<?= $i ?>][h]" type="text" value="<?= @$cal['h'] ?>" pattern="[0-9]{1,2}" size="1" placeholder="0~23"> Hr
               <input name="cal[<?= $i ?>][m]" type="text" value="<?= @$cal['m'] ?>" pattern="[0-9]{1,2}" size="1" placeholder="0~59"> mn
               <input name="cal[<?= $i ?>][temp]" type="text" value="<?= @$cal['temp'] ?>" pattern="[0-9]{2}(.[0-9]{1,2})?" size="1"> °C
         <?php
            }
            $i++;
         }
         ?>
         <br>
         après
         <input name="cal[<?= $i ?>][h]" type="text" value="" pattern="[0-9]{1,2}" size="1" placeholder="0~23"> Hr
         <input name="cal[<?= $i ?>][m]" type="text" value="" pattern="[0-9]{1,2}" size="1" placeholder="0~59"> mn
         <input name="cal[<?= $i ?>][temp]" type="text" value="" pattern="[0-9]{2}(.[0-9]{1,2})?" size="1"> °C
      </div>
      <br>
      <input type="submit" />
      <input type="button" value="Stop Refresh: 10" id="myrefresh" />
      <input type="button" value="Expert" id="expertbtn" />
      <br>
      <div class="d-none" id="expertdiv">
         Kp <input name="kp" type="text" value="<?= $config_ar[CFG_ROOM_NAME]['kp'] ?>" pattern="[0-9]{1,2}(.[0-9]{1,4})?" size="1">
         Ki <input name="ki" type="text" value="<?= $config_ar[CFG_ROOM_NAME]['ki'] ?>" pattern="[0-9]{1,2}(.[0-9]{1,4})?" size="1">
         Kd <input name="kd" type="text" value="<?= $config_ar[CFG_ROOM_NAME]['kd'] ?>" pattern="[0-9]{1,2}(.[0-9]{1,4})?" size="1">
         <br>
         MAC: <input name="frommac" type="text" value="<?= issetOr($MAC, '') ?>" size="12">
         <br>
         On Allowed:
         <input name="onallowed" type="hidden" value="0" />
         <input name="onallowed" type="checkbox" value="1" id="onallowed" <?= issetOr($config_ar[CFG_ROOM_NAME]['onallowed'], 0) ? 'checked="checked"' : '' ?>>
         <br>
         Off Allowed:
         <input name="offallowed" type="hidden" value="0" />
         <input name="offallowed" type="checkbox" value="1" id="offallowed" <?= issetOr($config_ar[CFG_ROOM_NAME]['offallowed'], 0) ? 'checked="checked"' : '' ?>>
         <br>
         Debug UDP 53000 enabled:
         <input name="udp_enabled" type="hidden" value="0" />
         <input name="udp_enabled" type="checkbox" value="1" id="udp_enabled" <?= issetOr($config_ar[CFG_ROOM_NAME]['udp_enabled'], 0) ? 'checked="checked"' : '' ?>>
         <br>
         <pre><?= print_r($status) ?></pre>
      </div>
   </form>

   <a href="stats.php?from=-12Hours&amp;mac=<?= $MAC ?>">Graphique</a>

   <script>
      var refresh = 9;

      function later() {
         refresh += 100;
         $('#myrefresh').val('Stop Refresh: 0' + refresh);
      }
      setInterval(function() {
         $('#myrefresh').val('Stop Refresh: 0' + refresh);
         if (0 == refresh) {
            window.location.href = window.location.href;
         }
         refresh--;
      }, 1000);
      $('#myrefresh, input').on('click', later);
      $('#expertbtn').on('click', () => {
         $('#expertdiv').toggleClass('d-none')
      });
   </script>

</body>

</html>