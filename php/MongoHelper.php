<?php

use MongoDB\Client;
use MongoDB\MongoClient;
use MongoDB\BSON\UTCDateTime;
use MongoDB\Client as Mongo;

if (!defined('HOME')) define("HOME", __DIR__ . '');
require_once(HOME . '/vendor/autoload.php');
require_once(HOME . '/vendor/mongodb/mongodb/src/functions.php');
require_once(HOME . '/credentials.php');

/**
 * class
 */
class MongoHelper
{
   private static $client;

   public static function GetClient($DB = '')
   {
      if (NULL == static::$client) {
         $user = MONGO_USER;
         $pwd = MONGO_PWD;
         static::$client = new MongoDB\Client('mongodb+srv://' . $user . ':' . $pwd . '@cluster0.csry5.mongodb.net/' . $DB . '?retryWrites=true&w=majority');
      }
      return static::$client;
   }
}
