<?php

/**
 * Contains PhonePePaymentMethodController.
 */

/**
 * An PhonePe payment method.
 */
class PhonePePaymentMethodController extends PaymentMethodController {

  /**
   * The production server.
   */
  const SERVER_PRODUCTION = 0;

  /**
   * The testing server.
   */
  const SERVER_TESTING = 1;

  /**
   * The production server URL.
   */
  const SERVER_PRODUCTION_URL = 'https://mercury.phonepe.com';

  /**
   * The testing server URL.
   */
  const SERVER_TESTING_URL = 'https://mercury-uat.phonepe.com/v3/debit';

  public $controller_data_defaults = array(
    'merchantid' => '',
    'merchantuserid' => '',
    'apikey1' => '',
    'apikey2' => '',
    'apikey3' => '',
    'apikey4' => '',
    'apikey5' => '',
    'apikey6' => '',
    'apikey7' => '',
    'apikey8' => '',
    'server' => self::SERVER_PRODUCTION,
  );

  public $payment_method_configuration_form_elements_callback = 'phonepe_payment_method_configuration_form_elements';

  /**
   * Implements PaymentMethodController::execute().
   */
  function execute(Payment $payment) {
    entity_save('payment', $payment);
    $_SESSION['phonepe_pid'] = $payment->pid;
    drupal_goto('phonepe/redirect/' . $payment->pid);
  }

  /**
   * Returns the API server URL.
   *
   * @throws InvalidArgumentException
   *
   * @param int $server
   *   self::SERVER_PRODUCTION or self::SERVER_TESTING.
   *
   * @return string
   */
  function debitURL($server) {
    $urls = array(
      $this::SERVER_PRODUCTION => $this::SERVER_PRODUCTION_URL,
      $this::SERVER_TESTING => $this::SERVER_TESTING_URL,
    );
    if (array_key_exists($server, $urls)) {
      return url($urls[$server], array(
        'external' => TRUE,
      ));
    }
    else {
      throw new InvalidArgumentException(t('Server type does not exist.'));
    }
  }

    /**
   * Returns the Key index and Key.
   *
   * @throws InvalidArgumentException
   *
   *
   * @return array
   */
  function saltKey(Payment $payment) {
     //randomize hash key
    $saltkeys =array();
    array_push($saltkeys,$payment->method->controller_data['apikey1']);
    array_push($saltkeys,$payment->method->controller_data['apikey2']);
    if(!empty($payment->method->controller_data['apikey3'])) 
      array_push($saltkeys,$payment->method->controller_data['apikey3']);
    if(!empty($payment->method->controller_data['apikey4'])) 
      array_push($saltkeys,$payment->method->controller_data['apikey4']);
    if(!empty($payment->method->controller_data['apikey5'])) 
      array_push($saltkeys,$payment->method->controller_data['apikey5']);
    if(!empty($payment->method->controller_data['apikey6'])) 
      array_push($saltkeys,$payment->method->controller_data['apikey6']);
    if(!empty($payment->method->controller_data['apikey7'])) 
      array_push($saltkeys,$payment->method->controller_data['apikey7']);
    if(!empty($payment->method->controller_data['apikey8'])) 
      array_push($saltkeys,$payment->method->controller_data['apikey8']); 

    if (!empty($saltkeys)) {
      $saltkey = array();
      $saltindex = array_rand($saltkeys,1); 
      $saltkey[$saltindex+1] = $saltkeys[$saltindex];
      return $saltkey;
    }
    else {
      throw new InvalidArgumentException(t('Invalid Salt Key.'));
    }
  }

  /**
   * Returns a map of PhonePe statuses to Payment statuses.
   *
   * @return array
   *   Keys are PhonePe statuses, values are Payment statuses.
   */
  function statusMap() {
    return array(
      //"Payment was not initiated after coming to PhonePe container".
      'TRANSACTION_NOT_FOUND' => PAYMENT_STATUS_UNKNOWN,
      //"The request is not valid".
      'BAD_REQUEST' => PAYMENT_STATUS_UNKNOWN,
      //"X-VERIFY header is incorrect".
      'AUTHORIZATION_FAILED'	 => PAYMENT_STATUS_AUTHORIZATION_FAILED,
      //"Something went wrong".
      'INTERNAL_SERVER_ERROR' => PAYMENT_STATUS_FAILED,
      //"Payment is successful".
      'PAYMENT_SUCCESS' => PAYMENT_STATUS_SUCCESS,
      //"Payment failed".
      'PAYMENT_ERROR' => PAYMENT_STATUS_FAILED,
      //"Payment failed".
      'PAYMENT_FAILED' => PAYMENT_STATUS_FAILED,
      //"Payment is pending".
      'PAYMENT_PENDING' => PAYMENT_STATUS_PENDING,
      //"Payment cancelled by merchant".
      'PAYMENT_CANCELLED' => PAYMENT_STATUS_CANCELLED,
      //"Payment declined by user".
      'PAYMENT_DECLINED' => PAYMENT_STATUS_FAILED,
    );
  }

  /**
   * Converts an PhonePe status to a Payment status.
   *
   * @param string $phonepe_status
   *
   * @return string
   */
  function convertStatus($phonepe_status) {
    $status_map = $this->statusMap();
    if (array_key_exists($phonepe_status,$status_map))
       return $status_map[$phonepe_status];
    return PAYMENT_STATUS_UNKNOWN;
  }

  

  /**
   * Sets up redirect POST data.
   *
   * @param Payment $payment
   *
   * @return array
   *   Keys are POST parameter names, values are values.
   */
  function phonepePostData(Payment $payment) {

    $return_url = url('phonepe/return', array(
      'absolute' => TRUE,
    ));
    //currency check
    $merchantid = $payment->method->controller_data['merchantid']; 
    $transactionId ='TX'.$payment->method->pmid."PY".$payment->pid;
    $orderId =$payment->context_data['order_id'];
    $merchantUserId = (string)$payment->uid; 
    //convert to paisa
    $amount =  $payment->totalAmount(TRUE)*100;
    
     $data = array(
       "merchantId"=>$merchantid, 
       "transactionId"=>$transactionId,
       "merchantUserId"=> $merchantUserId,
       "amount"=>$amount,
       "merchantOrderId"=>$orderId,
     );

     //get salt key and index
     $salt = $this->saltKey($payment);
     $salt_key = reset($salt); 
     $salt_index = key($salt); 

     $encodedRequest = base64_encode(json_encode($data));
     $SHAval = hash('sha256',  $encodedRequest."/v3/debit".$salt_key);
     $SHAval .= "###".$salt_index;
 
    $requestPayload =  json_encode(array(
      "request"=>$encodedRequest
    ));
  
     $postData['server']= $this->debitURL($payment->method->controller_data['server']);
     $postData['x-callback-url']= $return_url;
     $postData['x-verify']=$SHAval;
     $postData['request']=$requestPayload;
     return $postData;
  }


   /**
   * Gets PhonePe response data.
   *
   * @return array
   */
  function responseData() {
    $data_raw = array_merge($_POST);
    unset($data_raw['q']);
    return $data_raw;
  } 

  /**
   * Check PhonePe Status.
   *
   * @return string|bool $string_or_boolean
   */
  function checkTransactionStatus(array $data, Payment $payment) {


    $checksum = $data['checksum'];
    unset($data['checksum']);

    $cs_keyvalue = explode('###',$checksum);
    $cs_index = 'apikey'.$cs_keyvalue[1];
    $salt = $payment->method->controller_data[$cs_index];

    $hash_value = implode('',$data);
    $hash_value = $hash_value.$salt;
    
    $SHAval = hash('sha256',$hash_value);

    if($cs_keyvalue[0] ==$SHAval){
      return TRUE;
    }
    return FALSE;
}

/**
   * Processes PhonePe Process Payment.
   *
   * @return string|bool $string_or_boolean
   */
  function processPhonePePayment(array $data, Payment $payment) {
    $payment->setStatus(new PaymentStatusItem($this->convertStatus($data['code'])));
    entity_save('payment', $payment);
    module_invoke_all('phonepe_process_payment', $data, $payment);
    $payment->finish();
  }

}