<?php
require_once('lib.php');
require_once('MyMongo.php');

function get_last_level_alert($config_ar)
{
   $daily_performance = MyMongo::GetPerformance(issetOr($_GET['from'], '-12 hours'), issetOr($config_ar[CFG_ROOM_NAME]['frommac'], ''));

   $data_nowtemp =
      array_map(function ($k, $v) {
         return ['x' => $k, 'y' => round($v['nowtemp'], 2)];
      }, array_keys($daily_performance), array_values($daily_performance));

   $data_settemp =
      array_map(function ($k, $v) {
         return ['x' => $k, 'y' => round($v['settemp'], 2)];
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
   $last_alert = NULL;
   for ($i = 1; $i < count($daily_performance); $i++) {
      if ($data_nowtemp[$i]['y'] > $data_settemp[$i]['y']) {
         $is_alert = false;
         $last_alert = NULL;
      }
      if (
         $data_nowpercent[$i]['y'] >= 19
         && !empty($last)
         && $data_nowtemp[$i]['y'] < $data_settemp[$i]['y']
         && (array_sum($avg_ar) / $avg_idx) > $data_nowtemp[$i]['y']
         && !$is_alert
      ) {
         if ((new DateTime($data_nowtemp[$last]['x']))->add(new DateInterval('PT13M')) < (new DateTime($data_nowtemp[$i]['x']))) {
            $last_alert = $data_nowtemp[$i]['x'];
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
   return $last_alert;
}
