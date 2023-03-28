<?php
$configFilePath = "config.json";
$statusFilePath = "status.json";

require_once('credentials.php');
require_once('lib.php');
$status = json_decode(file_get_contents($statusFilePath), true);
try {
   //Get data from existing json file
   $jsonConfigStr = file_get_contents($configFilePath);
   // converts json data into array
   $config_ar = json_decode($jsonConfigStr, true);

   if (!(@$config_ar[CFG_ROOM_NAME]['manual'])) {
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
      $formdata[CFG_ROOM_NAME]['manual'] = (bool)$_POST['manual'];
      $formdata[CFG_ROOM_NAME]['frommac'] = $_POST['frommac'];
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

if (!(@$config_ar[CFG_ROOM_NAME]['manual'])) {
   // --- NEW CALENDAR ---
   processCalendar($config_ar);
}

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
      }
   </style>
</head>

<body>

   <?php
   require_once('MyMongo.php');
   $daily_performance = MyMongo::GetPerformance(issetOr($_GET['from'], '-1 days'), issetOr($config_ar[CFG_ROOM_NAME]['frommac'], ''));

   $data_nowtemp =
      array_map(function ($k, $v) {
         return ['x' => $k, 'y' => round($v['nowtemp'], 2)];
      }, array_keys($daily_performance), array_values($daily_performance));

   $data_settemp =
      array_map(function ($k, $v) {
         return ['x' => $k, 'y' => round($v['settemp'], 2)];
      }, array_keys($daily_performance), array_values($daily_performance));

   $data_kp =
      array_map(function ($k, $v) {
         return ['x' => $k, 'y' => round($v['kp'], 2)];
      }, array_keys($daily_performance), array_values($daily_performance));

   $data_ki =
      array_map(function ($k, $v) {
         return ['x' => $k, 'y' => round($v['ki'], 2)];
      }, array_keys($daily_performance), array_values($daily_performance));

   $data_kd =
      array_map(function ($k, $v) {
         return ['x' => $k, 'y' => round($v['kd'], 2)];
      }, array_keys($daily_performance), array_values($daily_performance));

   $data_pterm =
      array_map(function ($k, $v) {
         return ['x' => $k, 'y' => round($v['pterm'], 2)];
      }, array_keys($daily_performance), array_values($daily_performance));

   $data_iterm =
      array_map(function ($k, $v) {
         return ['x' => $k, 'y' => round($v['iterm'], 2)];
      }, array_keys($daily_performance), array_values($daily_performance));

   $data_dterm =
      array_map(function ($k, $v) {
         return ['x' => $k, 'y' => round(-$v['dterm'], 2)];
      }, array_keys($daily_performance), array_values($daily_performance));

   $data_nowpercent =
      array_map(function ($k, $v) {
         $y = round($v['nowpercent'] * 20 / 100, 2);
         if ($v['rikastatus'] == 'EXIT')
            $y = -1;
         if ($v['rikastatus'] == 'OFF')
            $y = -2;
         return ['x' => $k, 'y' => $y];
      }, array_keys($daily_performance), array_values($daily_performance));

   $avg_idx = 0;
   $avg_ar = [];
   $last = 0;
   $is_alert = false;
   $alert_ar = [];
   for ($i = 1; $i < count($daily_performance); $i++) {
      if ($data_nowtemp[$i]['y'] > $data_settemp[$i]['y']) {
         $is_alert = false;
         $alert_ar = [];
      }
      if (
         $data_nowpercent[$i]['y'] >= 19
         && !empty($last)
         && $data_nowtemp[$i]['y'] < $data_settemp[$i]['y']
         && (array_sum($avg_ar) / $avg_idx) > $data_nowtemp[$i]['y']
         && !$is_alert
      ) {
         if ((new DateTime($data_nowtemp[$last]['x']))->add(new DateInterval('PT13M')) < (new DateTime($data_nowtemp[$i]['x']))) {
            $alert_ar[] = 'alert ' . $data_nowtemp[$i]['x'] . '<br>';
            $last = $i;
            $avg_idx = 0;
            $is_alert = true;
         }
      } else {
         $last = $i;
         $avg_idx = 0;
      }
      $avg_ar[$avg_idx] = $data_nowtemp[$i]['y'];
      if ($avg_idx == 30) // 30*10sec = last 5mn
         array_shift($avg_ar);
      else
         $avg_idx++;
   }
   foreach($alert_ar as $k => $v) {
      echo $v;
   }

   ?>
   <div class="">
      <div class="">
         <canvas id="jsChartScoreOverTime" width="375" height="375" style="display: block; box-sizing: border-box; height: 600px; width: 600px;max-height: 100vh"></canvas>
      </div>
      <script>
         var ctx = document.getElementById('jsChartScoreOverTime');
         var jsChartScoreOverTime = new Chart(ctx, {
            type: 'line',
            data: {
               datasets: [{
                  label: 'Température',
                  data: <?= json_encode($data_nowtemp) ?>,
                  backgroundColor: [
                     'rgba(54, 162, 235, 0.2)'
                  ],
                  borderColor: [
                     'rgba(54, 162, 235, 1)'
                  ],
                  borderWidth: 1
               }, {
                  label: 'Réglage',
                  data: <?= json_encode($data_settemp) ?>,
                  backgroundColor: [
                     'red'
                  ],
                  borderColor: [
                     'red'
                  ],
                  borderWidth: 1
               }, {
                  label: 'nowpercent',
                  data: <?= json_encode($data_nowpercent) ?>,
                  backgroundColor: [
                     'green'
                  ],
                  borderColor: [
                     'green'
                  ],
                  borderWidth: 1
               }, {
                  label: 'Kp',
                  data: <?= json_encode($data_kp) ?>,
                  backgroundColor: [
                     'brown'
                  ],
                  borderColor: [
                     'brown'
                  ],
                  borderWidth: 1,
                  hidden: true
               }, {
                  label: 'Ki',
                  data: <?= json_encode($data_ki) ?>,
                  backgroundColor: [
                     'pink'
                  ],
                  borderColor: [
                     'pink'
                  ],
                  borderWidth: 1,
                  hidden: true
               }, {
                  label: 'Kd',
                  data: <?= json_encode($data_kd) ?>,
                  backgroundColor: [
                     'purple'
                  ],
                  borderColor: [
                     'purple'
                  ],
                  borderWidth: 1,
                  hidden: true
               }, {
                  label: 'pTerm',
                  data: <?= json_encode($data_pterm) ?>,
                  backgroundColor: [
                     '#111111'
                  ],
                  borderColor: [
                     '#111111'
                  ],
                  borderWidth: 1,
                  hidden: true
               }, {
                  label: 'iTerm',
                  data: <?= json_encode($data_iterm) ?>,
                  backgroundColor: [
                     '#222222'
                  ],
                  borderColor: [
                     '#222222'
                  ],
                  borderWidth: 1,
                  hidden: true
               }, {
                  label: '-dTerm',
                  data: <?= json_encode($data_dterm) ?>,
                  backgroundColor: [
                     'orange'
                  ],
                  borderColor: [
                     'orange'
                  ],
                  borderWidth: 1,
                  hidden: true
               }]
            },
            options: {
               scales: {
                  y: {
                     beginAtZero: false
                  }
               },
               elements: {
                  point: {
                     radius: 0 // default to disabled in all datasets
                  }
               }
            }
         });
      </script>
   </div>

</body>

</html>