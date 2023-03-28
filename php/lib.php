<?php

function issetOr(&$var, $replacement)
{
   return (isset($var) ? $var : $replacement);
}

function emptyThen(&$var, $replacement)
{
   return (empty($var) ? $replacement : $var);
}

function processCalendar(&$config_ar)
{
   $now = (new DateTime())->setTimezone(new DateTimeZone('Europe/Paris'));
   $cal_ar = issetOr($config_ar[CFG_ROOM_NAME]['cal'], []);
   usort($cal_ar, "array_cal_cmp");
   foreach ($cal_ar as $cal) {
      if (!empty($cal['h']) && !empty($cal['temp'])) {
         $row_time = (new DateTime())->setTimezone(new DateTimeZone('Europe/Paris'))->setTime($cal['h'], emptyThen($cal['m'], 0)); //->sub(new DateInterval('PT1H'))
         $row_temp = (float)($cal['temp']);
         if ($now > $row_time) {
            $config_ar[CFG_ROOM_NAME]['setpoint'] = $row_temp;
            $found = true;
         }
      }
   }
   // use the last one if early morning
   if(empty($found)) {
      $config_ar[CFG_ROOM_NAME]['setpoint'] = $row_temp;
   }
}

if (!function_exists('fastcgi_finish_request')) {
   function fastcgi_finish_request()
   {
   }
}

function array_cal_cmp($a, $b)
{
   return (emptyThen($a["h"], 0) - emptyThen($b["h"], 0)) * 100 + (emptyThen($a["m"], 0) - emptyThen($b["m"], 0));
}

function isMode($mode)
{
   global $config_ar;
   return (((int)$config_ar[CFG_ROOM_NAME]['manual']) === (int)$mode);
}

function isAlert($decoded)
{
   if($decoded['nowtemp'] >= $decoded['settemp']
   || $decoded['nowpercent'] < 95
   || $decoded['rikastatus'] != 'ON')
   {
      return false;
   }
   $history_15mn = MyMongo::GetPerformance('-14 minutes', $decoded['mac']);

   $data_nowtemp =
      array_map(function ($k, $v) {
         return ['x' => $k, 'y' => round($v['nowtemp'], 2)];
      }, array_keys($history_15mn), array_values($history_15mn));

   $data_settemp =
      array_map(function ($k, $v) {
         return ['x' => $k, 'y' => round($v['settemp'], 2)];
      }, array_keys($history_15mn), array_values($history_15mn));

   $data_nowpercent =
      array_map(function ($k, $v) {
         $y = round($v['nowpercent'], 2);
         return ['x' => $k, 'y' => $y];
      }, array_keys($history_15mn), array_values($history_15mn));

   $avg_idx = 0;
   $avg_ar = [];
   $last = 0;
   for($i = 1; $i < count($history_15mn); $i++)
   {
      if( $data_nowpercent[$i]['y'] >= 95
         && !empty($last)
         && $data_nowtemp[$i]['y'] < $data_settemp[$i]['y']
         && (array_sum($avg_ar)/$avg_idx) > $data_nowtemp[$i]['y'])
      {
         if((new DateTime($data_nowtemp[$last]['x']))->add(new DateInterval('PT13M')) < (new DateTime($data_nowtemp[$i]['x'])))
         {
            return true;
            $last = $i;
            $avg_idx = 0;
         }
      } else {
         $last = $i;
         $avg_idx = 0;
      }
      $avg_ar[$avg_idx] = $data_nowtemp[$i]['y'];
      if($avg_idx == 30) // 30*10sec = last 5mn
         array_shift($avg_ar);
      else
         $avg_idx++;
   }
   return false;
}