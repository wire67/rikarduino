<?php
use MongoDB\Client;
use MongoDB\MongoClient;
use MongoDB\BSON\UTCDateTime;
use MongoDB\Client as Mongo;

define('ESP_HANDLE', CFG_ROOM_NAME);

if (!defined('HOME')) define("HOME", __DIR__ . '');
require_once(HOME . '/vendor/autoload.php');
require_once(HOME . '/vendor/mongodb/mongodb/src/functions.php');
require_once('MongoHelper.php');

define('ONE_DAY_IN_SECONDS', 1 * 24 * 3600);

function safeIncrement(&$var){
   $var = ($var??0)+1;
}

/**
 * class
 */
class MyMongo
{
   private const DB = 'Esp32';
   private static $orders;

   /**
    *
    */
   public static function PushStatus($inputData)
   {
      $client = MongoHelper::GetClient(static::DB);
      $collectionName = ESP_HANDLE;
      $ret = $client->{static::DB}->{$collectionName}->insertOne(
         [
         'owner' => ESP_HANDLE,
         'at' => new UTCDateTime(),
         'data' => $inputData,
         ]
      );
      error_log(json_encode($ret, JSON_PRETTY_PRINT));
   }

   /**
    * Get Product Performance
    */
    public static function GetAllTemps($from, $mac)
    {
       if(empty(static::$orders)) {
         $client = MongoHelper::GetClient(static::DB);
         $collectionName = ESP_HANDLE;
         $filter = [
            'owner' => ESP_HANDLE,
            'at' => ['$gte' => new UTCDateTime($from * 1000)],
            'data.mac' => $mac
         ];
         static::$orders = $client->{static::DB}->{$collectionName}->find($filter)->toArray();
       }
       return static::$orders;
    }

   /**
    * Get Daily Performance
    */
   public static function GetPerformance($since, $mac)
   {
      $from = strtotime($since);
      $ret = MyMongo::GetAllTemps($from, $mac);
      $ar = [];
      foreach ($ret as $key => $val) {
         $at = $val['at']->toDateTime()->format('Y-m-d H:i:s');
         $ar[$at] = $val['data'];
      }
      ksort($ar);
      return $ar;
   }
}
