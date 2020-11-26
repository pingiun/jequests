(import cjequests)

(def json
  (try (require "json")
     ([err] ())))

(defn- add-decode-fn
  [tbl]
  (let [decode (get (get json 'decode) :value)]
    (if (= decode nil)
      tbl
      (put tbl :json
         (fn [self]
           (decode (self :text)))))))

(def- jencode
  (let [jencode (get (get json 'encode) :value)]
    (if (= jencode nil)
      (error "json module not installed")
      jencode)))


(defn- make-kw-key
  [[key value]]
  [(keyword (string/ascii-lower key)) value])

(defn request
  "A raw wrapper around the cjequests/request function."
  [method url &opt data headers]
  (let [request-headers (map (fn [[k v]] (string k ": " v)) (pairs headers))
        response (cjequests/request method url data request-headers)
        headers-arr (mapcat (fn [x]
                              (->>
                                (string/split ":" x 0 2)
                                (map string/trim)
                                (make-kw-key)))
                            (response :headers))
        headers (table (splice headers-arr))]
    (add-decode-fn (put response :headers headers-arr))))

(defn get
  "Perform a GET request to the specified url. Adds a method called :json to the
  table if we have the json module available which will try to decode any json
  in the response."
  [url]
  (request :get url))

(defn- make-url-encoded
  [data]
  (string/join
    (map (fn [[k v]]
           (string (cjequests/escape k) "=" (cjequests/escape v)))
         (pairs data))
    "&"))

(defn post
  "Make a post request using the application/x-www-form-urlencoded mime type."
  [url form]
  (let [data (make-url-encoded form)]
    (request :post url data)))

(defn post-raw
  "Make a post request with raw string/buffer data."
  [url data]
  (request :post url data))

(defn post-json
  "Make a post request with json data"
  [url js]
  (request :post url (jencode js) {:content-type "application/json; charset=utf-8"}))
